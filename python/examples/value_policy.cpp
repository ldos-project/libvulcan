// LLM-authored "EVOLVE block" compiled separately, dlopen'd by Python.

#include "vulcan.h"
#include <algorithm>

extern "C" void vulcan_configure_value(vulcan::feature_registry& registry,
                                       vulcan::store_config& store_cfg,
                                       vulcan::value_config& config) {
    auto rtt       = registry.global.lookup_f64("rtt");
    auto queue_len = registry.global.lookup_i64("queue_len");

    // EVOLVE-BLOCK-START
    store_cfg.add_listeners(rtt,       {vulcan::listeners::global::RollingWindow(5), vulcan::listeners::global::MinMax()});
    store_cfg.add_listeners(queue_len, {vulcan::listeners::global::RollingWindow(5), vulcan::listeners::global::MinMax()});

    config.set_value_fn([rtt, queue_len](const vulcan::feature_store& fs) -> double {
        double  rtt_max = fs.get_max(rtt);
        int64_t q_min   = fs.get_min(queue_len);
        if (rtt_max <= 0.0) return 1.0;
        return std::max(1.0, 1000.0 / rtt_max - static_cast<double>(q_min));
    });
    // EVOLVE-BLOCK-END
}
