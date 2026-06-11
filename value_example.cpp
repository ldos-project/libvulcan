#include "vulcan.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <algorithm>

int main() {
    vulcan::feature_registry registry;
    auto rtt       = registry.global.declare_f64("rtt", "RTT time in milliseconds");
    auto queue_len = registry.global.declare_i64("queue_len", "Queue length in packets");
    auto prev_cwnd = registry.global.declare_f64("prev_cwnd", "Congestion window size returned by the previous decision");

    vulcan::value_config config1, config2;

    config1.set_information(
        "You are building a congestion control policy for a network flow. "
        "Each time the policy is invoked, it should return the next congestion window (cwnd) size in packets. "
        "You have access to RTT measurements, queue length, and the previous cwnd value to guide your decision."
    );

    // Store configs for each flow variant
    vulcan::store_config store_cfg1, store_cfg2;

    // EVOLVE-BLOCK-START
    store_cfg1.add_listeners(rtt, {vulcan::listeners::global::RollingWindow(5), vulcan::listeners::global::MinMax()});
    store_cfg1.add_listeners(queue_len, {vulcan::listeners::global::RollingWindow(5), vulcan::listeners::global::MinMax()});
    auto flow1_cwnd_fn = [&](const vulcan::feature_store& fs) -> double {
        double  rtt_max = fs.get_max(rtt);
        int64_t q_min   = fs.get_min(queue_len);
        if (rtt_max <= 0.0) return 1.0;
        return std::max(1.0, 1000.0 / rtt_max - static_cast<double>(q_min));
    };

    store_cfg2.add_listeners(rtt, {vulcan::listeners::global::RollingPercentile(100)});
    auto flow2_cwnd_fn = [&](const vulcan::feature_store& fs) -> double {
      return fs.get_percentile(rtt, 95);
    };
    config1.set_value_fn(flow1_cwnd_fn);
    config2.set_value_fn(flow2_cwnd_fn);
    // EVOLVE-BLOCK-END

    // Instantiate a policy for each flow.
    std::deque<vulcan::value_policy> policies;
    std::vector<std::shared_ptr<vulcan::feature_store>> stores;

    for(int i=0; i<10; i++) {
        if(i%2 == 0) {
            auto store = vulcan::make_shared_feature_store(registry, store_cfg1);
            policies.push_back(vulcan::instantiate_value_policy(registry, config1, store));
            stores.push_back(store);
        } else {
            auto store = vulcan::make_shared_feature_store(registry, store_cfg2);
            policies.push_back(vulcan::instantiate_value_policy(registry, config2, store));
            stores.push_back(store);
        }
    }

    std::cout << "\n" << policies.front().get_prompt() << std::endl;

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    for(int t=0; t<10; t++) {
        for(int j=0; j<2; j++) {
            if(stores[j]->has_listeners(rtt)) stores[j]->update(rtt, 50.0 + (std::rand() % 100));
            if(stores[j]->has_listeners(queue_len)) stores[j]->update(queue_len, static_cast<int64_t>(std::rand() % 20));
            double cwnd = vulcan::decision(policies[j]);
            if(stores[j]->has_listeners(prev_cwnd)) stores[j]->update(prev_cwnd, cwnd);
            std::cout << "[t= " << t << "] Flow# " << j << " cwnd= " << cwnd << "\n";
        }
    }
}
