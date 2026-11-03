#include "test_utils.hpp"
#include <cmath>
#include <iostream>

// Simple approx check
bool approx(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}

int main() {
    vulcan_test::run_test("RollingPercentile - global history", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto global_feat = registry.declare<double>("g_load", "Global Load", 0.0);
        config.add_listeners(global_feat, {vulcan::listeners::RollingPercentile(100)});   // window of 100
        auto store = vulcan::instantiate_feature_store(registry, config);

        // The initial value 0, then 1..99
        for (int i = 1; i < 100; ++i) {
            store.update(global_feat, static_cast<double>(i));
        }

        double p50 = store.get_temporal_percentile(global_feat, 0.5);
        double p90 = store.get_temporal_percentile(global_feat, 0.9);
        double p0 = store.get_temporal_percentile(global_feat, 0.0);
        double p100 = store.get_temporal_percentile(global_feat, 1.0);

        // median of 0..99 is 49 or 50. Our implementation is p * (size-1) idx.
        // 0.5 * 99 = 49.5 -> 49 (floor) due to static_cast. 
        // Or if tree uses find_by_order(49). Value at 49 is 49.
        
        TEST_ASSERT(approx(p50, 49.0) || approx(p50, 50.0), "Global P50 should be around 49-50, got " + std::to_string(p50));
        TEST_ASSERT(approx(p90, 89.0) || approx(p90, 90.0), "Global P90 should be around 89-90, got " + std::to_string(p90));
        TEST_ASSERT(approx(p0, 0.0), "Global Min should be 0");
        TEST_ASSERT(approx(p100, 99.0), "Global Max should be 99");

        // Test Sliding Window (Add 100..199, pushing out 0..99)
        for (int i = 100; i < 200; ++i) {
             store.update(global_feat, static_cast<double>(i));
        }
        
        // Window should be 100..199
        double p50_new = store.get_temporal_percentile(global_feat, 0.5);
        TEST_ASSERT(p50_new >= 149.0, "Global P50 after slide should be ~149, got " + std::to_string(p50_new));
    });

    vulcan_test::run_test("RollingPercentile - float features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float", 1.0f);
        config.add_listeners(x, {vulcan::listeners::RollingPercentile(4)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (float v : {4.0f, 2.0f, 3.0f})
            features.update(x, v);
        TEST_ASSERT(features.get_temporal_percentile(x, 1.0) == 4.0, "p100 is 4");
    });

    vulcan_test::run_test("RollingPercentile - int32 features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.declare<int32_t>("n", "Test int32", -5);
        config.add_listeners(n, {vulcan::listeners::RollingPercentile(4)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {10, 3, 2147483647})
            features.update(n, v);
        TEST_ASSERT(features.get_temporal_percentile(n, 0.0) == -5.0, "p0 is -5");
    });

    vulcan_test::run_test("RollingPercentile - bool features are rejected at construction", []() {
        vulcan::feature_registry registry;
        auto g = registry.declare<bool>("g", "Global flag", false);
        vulcan::store_config config;
        config.add_listeners(g, {vulcan::listeners::RollingPercentile(4)});
        TEST_ASSERT(vulcan_test::construction_throws(registry, config), "RollingPercentile on bool should be rejected");
    });

    return 0;
}
