#include "test_utils.hpp"
#include <iostream>

int main() {
    vulcan_test::run_test("MinMax and Average", []() {
        // Configure for MAX tracking on var1 with window 5
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var1 = registry.global.declare_f64("var1", "Test feature");
        config.add_listeners(var1, {vulcan::listeners::global::MinMax(), vulcan::listeners::global::Average()});
        
        auto features = vulcan::instantiate_feature_store(registry, config);

        double sum = 0;
        for(int i = 0; i < 10; i++) {
            double val = static_cast<double>(i);
            features.update(var1, val);
            sum += val;            
            double current_max = features.get_max(var1);
            double current_avg = features.get_avg(var1);

            double expected_max = val; // Strictly increasing, so max is always current
            
            // Calculate expected avg manually (GLOBAL)
            double expected_avg = sum / (double)(i + 1);
            
            TEST_ASSERT(current_max == expected_max, "Max value mismatch");
            TEST_ASSERT(std::abs(current_avg - expected_avg) < 1e-6, "Avg value mismatch: got " + std::to_string(current_avg) + " expected " + std::to_string(expected_avg));
        }
    });
    return 0;
}
