// LLM-authored "EVOLVE block" compiled separately, dlopen'd by Python.

#include "vulcan.h"

extern "C" void vulcan_configure_rank(vulcan::feature_registry& registry,
                                      vulcan::store_config& store_cfg,
                                      vulcan::rank_config& config) {
    auto system_load    = registry.global.lookup_f64("system_load");
    auto latency        = registry.object.lookup_f64("latency");
    auto temp           = registry.object.lookup_i64("temp");
    auto prev_decisions = registry.global.lookup_i64("prev_decisions");

    // EVOLVE-BLOCK-START
    store_cfg.add_listeners(system_load,    {vulcan::listeners::global::RollingWindow(1)});
    store_cfg.add_listeners(latency,        {vulcan::listeners::object::RollingWindow(5)});
    store_cfg.add_listeners(temp,           {vulcan::listeners::object::RollingWindow(5)});
    store_cfg.add_listeners(prev_decisions, {vulcan::listeners::global::RollingWindow(1)});

    config.set_scoring_fn([latency, temp, prev_decisions]
                          (const vulcan::feature_store& fs, int64_t obj_id) -> double {
        double l = fs.get_latest(latency, obj_id);
        double t = fs.get_latest(temp, obj_id);
        double score = l + t;
        if (fs.get_latest(prev_decisions) != obj_id) score += 20;
        return score;
    });
    config.set_comparator(vulcan::min);
    config.set_sorting_function(vulcan::rank::FullSort);
    // EVOLVE-BLOCK-END
}
