// LLM-authored "EVOLVE block" compiled separately, dlopen'd by Python.
// The scaffolding (feature declarations, object management, decision loop)
// lives in the Python driver; only the policy logic below is LLM-generated.

#include "vulcan.h"

extern "C" void vulcan_configure_rank(vulcan::feature_registry& registry,
                                      vulcan::rank_config& config) {
    auto system_load    = registry.global.lookup_f64("system_load");
    auto latency        = registry.object.lookup_f64("latency");
    auto temp           = registry.object.lookup_i64("temp");
    auto prev_decisions = registry.global.lookup_i64("prev_decisions");

    // EVOLVE-BLOCK-START
    config.add_listeners(system_load,    {vulcan::listeners::global::RollingWindow(1)});
    config.add_listeners(latency,        {vulcan::listeners::object::RollingWindow(5)});
    config.add_listeners(temp,           {vulcan::listeners::object::RollingWindow(5)});
    config.add_listeners(prev_decisions, {vulcan::listeners::global::RollingWindow(1)});

    config.set_scoring_fn([latency, temp, prev_decisions]
                          (const vulcan::feature_store& fs, int64_t obj_id) -> double {
        double l = fs.get_latest(latency, obj_id);
        double t = fs.get_latest(temp, obj_id);
        double score = l + t;
        if (fs.get_latest(prev_decisions) != obj_id) score += 20; // switching overhead
        return score;
    });
    config.set_comparator(vulcan::min);
    config.set_sorting_function(vulcan::rank::FullSort);
    // EVOLVE-BLOCK-END
}
