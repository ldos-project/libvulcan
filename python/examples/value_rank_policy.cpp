// LLM-authored "EVOLVE block" for a two-stage I/O dispatch policy.
//
// Stage 1 (VALUE): Should we issue a READ now, or WAIT?
// Stage 2 (RANK): If READ, which SSD should we target?

#include "vulcan.h"

extern "C" void vulcan_configure_value(vulcan::feature_registry& registry,
                                       vulcan::store_config& store_cfg,
                                       vulcan::value_config& config) {
    auto io_queue_depth = registry.global.lookup_f64("io_queue_depth");
    auto system_iops    = registry.global.lookup_f64("system_iops");

    // EVOLVE-BLOCK-START
    store_cfg.add_listeners(io_queue_depth, {vulcan::listeners::global::RollingWindow(3),
                                             vulcan::listeners::global::EWMA({0.3})});
    store_cfg.add_listeners(system_iops,    {vulcan::listeners::global::RollingWindow(3),
                                             vulcan::listeners::global::MinMax()});

    config.set_value_fn([io_queue_depth, system_iops](const vulcan::feature_store& fs) -> double {
        double queue_ewma = fs.get_ewma(io_queue_depth, 0.3);
        double iops_now   = fs.get_latest(system_iops);
        double iops_max   = fs.get_max(system_iops);
        if (queue_ewma > 60.0 && iops_now < iops_max * 0.5) return 0.0;
        return 1.0;
    });
    // EVOLVE-BLOCK-END
}

extern "C" void vulcan_configure_rank(vulcan::feature_registry& registry,
                                      vulcan::store_config& store_cfg,
                                      vulcan::rank_config& config) {
    auto ssd_latency   = registry.object.lookup_f64("ssd_latency");
    auto ssd_queue_len = registry.object.lookup_i64("ssd_queue_len");

    // EVOLVE-BLOCK-START
    store_cfg.add_listeners(ssd_latency,    {vulcan::listeners::object::RollingWindow(3),
                                             vulcan::listeners::object::EWMA({0.5})});
    store_cfg.add_listeners(ssd_queue_len,  {vulcan::listeners::object::RollingWindow(3)});

    config.set_scoring_fn([ssd_latency, ssd_queue_len]
                          (const vulcan::feature_store& fs, int64_t obj_id) -> double {
        double lat_ewma = fs.get_ewma(ssd_latency, obj_id, 0.5);
        int64_t qlen    = fs.get_latest(ssd_queue_len, obj_id);
        return lat_ewma + qlen * 2.0;
    });
    config.set_comparator(vulcan::min);
    config.set_sorting_function(vulcan::rank::FullSort);
    // EVOLVE-BLOCK-END
}
