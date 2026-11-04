// Load balancing: split traffic across regions.
//
// Output type: std::map<region, double>. The function gives each region a weight; the mechanism sends each
// region its share of the traffic.

#include "vulcan.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

// ---- Trusted compute base: groups, features, and the decision's prototype ----
VULCAN_GROUP(region, "Regions that serve traffic");

vulcan::feature_registry registry = [] {
    vulcan::feature_registry r;
    r.declare_group<region>();
    return r;
}();

const vulcan::feature_handle<double, region> latency =
    registry.declare<double, region>("latency", "Mean request latency in ms over the last tick", 0.0);

std::map<region, double> weights(const vulcan::feature_store& fs);

// ---- The developer's prompt: the task, what the policy can read, and the functions to write ----
std::string prompt() {
    return "--- Task ---\nSplit incoming traffic across regions.\n\n" + registry.describe() +
           "--- Write ---\n"
           "1. void configure(vulcan::store_config& listeners): attach the listeners your function reads.\n"
           "2. std::map<region, double> weights(const vulcan::feature_store& fs)\n"
           "   Return each region's share of the traffic; the shares should sum to 1.\n";
}

// ---- Synthesized by the LLM ----
// EVOLVE-BLOCK-START
void configure(vulcan::store_config& listeners) {
    listeners.add_listener(latency, vulcan::listeners::EWMA({0.3}));
}

// Weight each region by the inverse of its smoothed latency, normalized to sum to 1
std::map<region, double> weights(const vulcan::feature_store& fs) {
    std::map<region, double> w;
    double total = 0.0;
    for (region r : fs.all<region>()) {
        w[r] = 1.0 / (fs.get_ewma(latency, r, 0.3) + 1.0);
        total += w[r];
    }
    for (auto& [r, weight] : w) weight /= total;
    return w;
}
// EVOLVE-BLOCK-END

// ---- Mechanism: four regions; region 3 degrades over time ----
double simulated_latency(int64_t r, int t) { return r == 3 ? 20.0 + 15.0 * t : 20.0 + 5.0 * r; }

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--prompt") {
        std::cout << prompt();
        return 0;
    }
    vulcan::store_config listeners;
    configure(listeners);
    auto store = vulcan::instantiate_feature_store(registry, listeners);
    for (int64_t r = 0; r < 4; ++r) store.add(region{r}, {{latency, simulated_latency(r, 0)}});

    std::cout << std::fixed << std::setprecision(2);
    for (int t = 1; t <= 5; ++t) {
        for (int64_t r = 0; r < 4; ++r) store.update(latency, region{r}, simulated_latency(r, t));
        std::cout << "[t=" << t << "] traffic share:";
        for (const auto& [r, share] : weights(store))
            std::cout << "  region " << static_cast<int64_t>(r) << " " << share;
        std::cout << "\n";
    }
    return 0;
}
