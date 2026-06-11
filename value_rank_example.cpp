// Two-stage I/O dispatch policy using libvulcan.
//
// Stage 1 (VALUE): Should we issue a READ now, or WAIT?
//   - Uses system-wide signals (I/O queue depth, IOPS) to decide.
//
// Stage 2 (RANK): If READ, which SSD should we target?
//   - Scores each drive by latency + queue length, picks the best.
//
// Both stages share a single feature_store.

#include "vulcan.h"
#include <iostream>
#include <vector>
#include <string>

// --- Simulated environment ---
double get_io_queue_depth(int t) {
    return (t % 2 == 0) ? 80.0 + t * 5.0 : 20.0 - t * 2.0;
}

double get_system_iops(int t) {
    return 1000.0 - t * 100.0;
}

double get_ssd_latency(int ssd_id, int t) {
    switch (ssd_id) {
        case 1: return 5.0 + t * 15.0;
        case 2: return 50.0 - t * 5.0;
        case 3: return 30.0 + (t % 3) * 10.0;
        default: return 100.0;
    }
}

int get_ssd_queue_len(int ssd_id, int t) {
    switch (ssd_id) {
        case 1: return 2 + t * 3;
        case 2: return 15 - t * 2;
        case 3: return 8 + (t % 2) * 4;
        default: return 20;
    }
}

int main() {
    vulcan::feature_registry registry;

    auto io_queue_depth = registry.global.declare_f64("io_queue_depth", "Pending I/O requests in the system queue");
    auto system_iops    = registry.global.declare_f64("system_iops", "Current system-wide IOPS throughput");
    auto ssd_latency    = registry.object.declare_f64("ssd_latency", "Read latency in ms for this SSD");
    auto ssd_queue_len  = registry.object.declare_i64("ssd_queue_len", "Pending requests queued for this SSD");

    vulcan::store_config store_cfg;
    vulcan::value_config vcfg;
    vulcan::rank_config rcfg;

    // EVOLVE-BLOCK-START
    store_cfg.add_listeners(io_queue_depth, {vulcan::listeners::global::RollingWindow(3), vulcan::listeners::global::EWMA({0.3})});
    store_cfg.add_listeners(system_iops,    {vulcan::listeners::global::RollingWindow(3), vulcan::listeners::global::MinMax()});
    store_cfg.add_listeners(ssd_latency,    {vulcan::listeners::object::RollingWindow(3), vulcan::listeners::object::EWMA({0.5})});
    store_cfg.add_listeners(ssd_queue_len,  {vulcan::listeners::object::RollingWindow(3)});

    vcfg.set_value_fn([&](const vulcan::feature_store& fs) -> double {
        double queue_ewma = fs.get_ewma(io_queue_depth, 0.3);
        double iops_now   = fs.get_latest(system_iops);
        double iops_max   = fs.get_max(system_iops);
        if (queue_ewma > 60.0 && iops_now < iops_max * 0.5) return 0.0;
        return 1.0;
    });

    rcfg.set_scoring_fn([&](const vulcan::feature_store& fs, int64_t obj_id) -> double {
        double lat_ewma = fs.get_ewma(ssd_latency, obj_id, 0.5);
        int64_t qlen    = fs.get_latest(ssd_queue_len, obj_id);
        return lat_ewma + qlen * 2.0;
    });
    rcfg.set_comparator(vulcan::min);
    rcfg.set_sorting_function(vulcan::rank::FullSort);
    // EVOLVE-BLOCK-END

    vcfg.set_information(
        "Stage 1: decide whether to issue a READ now or WAIT based on system-wide "
        "I/O queue depth and IOPS headroom."
    );
    rcfg.set_information(
        "Stage 2: if READ is chosen, select the best SSD based on per-drive latency "
        "and queue length."
    );

    // Shared store — both policies see the same data.
    auto store = vulcan::make_shared_feature_store(registry, store_cfg);
    auto value_pol = vulcan::instantiate_value_policy(registry, vcfg, store);
    auto rank_pol  = vulcan::instantiate_rank_policy(registry, rcfg, store);

    std::vector<int> ssds = {1, 2, 3};
    for (int id : ssds) rank_pol.add_object(id);

    std::cout << value_pol.get_prompt() << "\n";
    std::cout << rank_pol.get_prompt() << "\n";

    for (int t = 0; t < 7; ++t) {
        if (store->has_listeners(io_queue_depth)) store->update(io_queue_depth, get_io_queue_depth(t));
        if (store->has_listeners(system_iops))    store->update(system_iops, get_system_iops(t));

        for (int ssd_id : ssds) {
            if (store->has_listeners(ssd_latency))   store->update(ssd_latency, ssd_id, get_ssd_latency(ssd_id, t));
            if (store->has_listeners(ssd_queue_len)) store->update(ssd_queue_len, ssd_id, static_cast<int64_t>(get_ssd_queue_len(ssd_id, t)));
        }

        double action = value_pol.decide();

        if (action < 0.5) {
            std::cout << "[t=" << t << "]  Decision: WAIT (queue saturated / low IOPS)\n";
        } else {
            int64_t best_ssd = vulcan::decision(rank_pol);
            std::cout << "[t=" << t << "]  Decision: READ -> SSD " << best_ssd << "\n";
        }
    }

    return 0;
}
