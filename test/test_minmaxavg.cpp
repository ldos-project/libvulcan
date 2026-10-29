#include "test_utils.hpp"
#include <iostream>

int main() {
    vulcan_test::run_test("MinMax and Average", []() {
        // Configure for MAX tracking on var1 with window 5
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var1 = registry.global.declare<double>("var1", "Test feature");
        config.add_listeners(var1, {vulcan::listeners::MinMax(), vulcan::listeners::Average()});
        
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

    vulcan_test::run_test("Int64 - MinMax is exact beyond 2^53", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.global.declare<int64_t>("n", "Large integer");
        config.add_listeners(n, {vulcan::listeners::MinMax()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        const int64_t a = 9007199254740993LL;   // 2^53 + 1, not representable as a double
        const int64_t b = 9007199254740995LL;   // 2^53 + 3
        features.update(n, a);
        features.update(n, b);
        TEST_ASSERT(features.get_max(n) == b, "Max is exact");
        TEST_ASSERT(features.get_min(n) == a, "Min is exact");
    });

    return 0;
}
