// Congestion control: the next congestion window for a flow.
//
// Output type: double. Two kinds of flow get their own synthesized function and listeners, so each is
// specialized to its kind; each flow has its own store.

#include "vulcan.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// ---- Trusted compute base: features and the decisions' prototypes ----
vulcan::feature_registry registry;

const vulcan::feature_handle<double> rtt =
    registry.declare<double>("rtt", "RTT in milliseconds", 0.0);
const vulcan::feature_handle<int64_t> queue_len =
    registry.declare<int64_t>("queue_len", "Queue length in packets", 0);

double cwnd_a(const vulcan::feature_store& fs);
double cwnd_b(const vulcan::feature_store& fs);

// ---- The developer's prompt for one kind of flow: the task, what it can read, and the functions to write ----
std::string prompt(const std::string& name) {
    return "--- Task ---\nReturn the next congestion window, in packets, for a network flow.\n\n" +
           registry.describe() +
           "--- Write ---\n"
           "1. void configure_" + name + "(vulcan::store_config& listeners): "
           "attach the listeners your function reads.\n"
           "2. double cwnd_" + name + "(const vulcan::feature_store& fs)\n"
           "   Return the congestion window, at least 1.\n";
}

// ---- Synthesized by the LLM: one function per kind of flow ----
// EVOLVE-BLOCK-START
void configure_a(vulcan::store_config& listeners) {
    listeners.add_listener(rtt, vulcan::listeners::RollingWindow(5)).add_listener(rtt, vulcan::listeners::MinMax());
    listeners.add_listener(queue_len, vulcan::listeners::RollingWindow(5))
        .add_listener(queue_len, vulcan::listeners::MinMax());
}

double cwnd_a(const vulcan::feature_store& fs) {
    double rtt_max = fs.get_max(rtt);
    if (rtt_max <= 0.0) return 1.0;
    return std::max(1.0, 1000.0 / rtt_max - static_cast<double>(fs.get_min(queue_len)));
}

void configure_b(vulcan::store_config& listeners) {
    listeners.add_listener(rtt, vulcan::listeners::RollingPercentile(100));
}

double cwnd_b(const vulcan::feature_store& fs) {
    return fs.get_temporal_percentile(rtt, 0.95);
}
// EVOLVE-BLOCK-END

// ---- Mechanism: two simulated flows, one of each kind ----
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--prompt") {
        std::cout << prompt("a") << "\n" << prompt("b");
        return 0;
    }
    vulcan::store_config listeners_a, listeners_b;
    configure_a(listeners_a);
    configure_b(listeners_b);

    std::vector<vulcan::feature_store> stores;
    stores.push_back(vulcan::instantiate_feature_store(registry, listeners_a));
    stores.push_back(vulcan::instantiate_feature_store(registry, listeners_b));

    std::srand(42);
    for (int t = 0; t < 10; t++) {
        for (int j = 0; j < 2; j++) {
            stores[j].update(rtt, 50.0 + (std::rand() % 100));
            stores[j].update(queue_len, static_cast<int64_t>(std::rand() % 20));
            double cwnd = j == 0 ? cwnd_a(stores[j]) : cwnd_b(stores[j]);
            std::cout << "[t= " << t << "] Flow# " << j << " cwnd= " << cwnd << "\n";
        }
    }
    return 0;
}
