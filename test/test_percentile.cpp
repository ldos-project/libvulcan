#include <iostream>
#include <cmath>
#include "vulcan.h"
#include "test_utils.hpp"

// Simple approx check
bool approx(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}

int main() {
    vulcan::feature_registry registry;
    vulcan::store_config config;
    
    // 1. Global Feature with Percentile History
    auto global_feat = registry.global.declare_f64("g_load", "Global Load");
    // Window of 100
    config.add_listeners(global_feat, {vulcan::listeners::global::RollingPercentile(100)});

    // 2. Object Feature with Population Distribution
    auto obj_feat = registry.object.declare_f64("o_temp", "Object Temp");
    config.add_listeners(obj_feat, {vulcan::listeners::object::PopulationPercentile()});

    // 3. Object Feature with Both Percentiles
    auto obj_combined_feat = registry.object.declare_f64("o_comb", "Object Combined");
    config.add_listeners(obj_combined_feat, {
        vulcan::listeners::object::RollingPercentile(100),
        vulcan::listeners::object::PopulationPercentile()
    });

    auto store = vulcan::instantiate_feature_store(registry, config);

    vulcan_test::run_test("Global Percentile History", [&]() {
        // Feed 0..99
        for (int i = 0; i < 100; ++i) {
            store.update(global_feat, static_cast<double>(i));
        }

        double p50 = store.get_percentile(global_feat, 0.5);
        double p90 = store.get_percentile(global_feat, 0.9);
        double p0 = store.get_percentile(global_feat, 0.0);
        double p100 = store.get_percentile(global_feat, 1.0);

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
        double p50_new = store.get_percentile(global_feat, 0.5);
        TEST_ASSERT(p50_new >= 149.0, "Global P50 after slide should be ~149, got " + std::to_string(p50_new));
    });

    vulcan_test::run_test("Object Population Distribution", [&]() {
        // Initial state:
        // Obj 1: 10
        // Obj 2: 20
        // Obj 3: 30
        store.update(obj_feat, 1, 10.0);
        store.update(obj_feat, 2, 20.0);
        store.update(obj_feat, 3, 30.0);

        // Median should be 20
        double dist_p50 = store.get_percentile(obj_feat, 0.5);
        TEST_ASSERT(approx(dist_p50, 20.0), "Population P50 should be 20, got " + std::to_string(dist_p50));

        // Update Obj 1: 10 -> 100
        // New state: 20, 30, 100
        // Median should be 30
        store.update(obj_feat, 1, 100.0);
        
        dist_p50 = store.get_percentile(obj_feat, 0.5);
        TEST_ASSERT(approx(dist_p50, 30.0), "Population P50 after update should be 30, got " + std::to_string(dist_p50));

        // Add Obj 4: 5
        // State: 5, 20, 30, 100
        // Median (0.5 * 3 = 1.5 -> idx 1) -> 20
        store.update(obj_feat, 4, 5.0);
        dist_p50 = store.get_percentile(obj_feat, 0.5);
        TEST_ASSERT(approx(dist_p50, 20.0), "Population P50 (4 items) should be 20, got " + std::to_string(dist_p50));
    });

    vulcan_test::run_test("Combined Object Percentiles (Rolling + Population)", [&]() {
        // Emit values
        store.update(obj_combined_feat, 1, 10.0);
        store.update(obj_combined_feat, 2, 20.0);
        store.update(obj_combined_feat, 1, 50.0);

        // Object 1 history [10.0, 50.0], P50 is 10.0 or 50.0
        double obj1_p50 = store.get_percentile(obj_combined_feat, 1, 0.5);
        TEST_ASSERT(approx(obj1_p50, 10.0) || approx(obj1_p50, 50.0), "Obj1 Rolling P50 should be 10 or 50, got " + std::to_string(obj1_p50));

        // Object 2 history [20.0], P50 is 20.0
        double obj2_p50 = store.get_percentile(obj_combined_feat, 2, 0.5);
        TEST_ASSERT(approx(obj2_p50, 20.0), "Obj2 Rolling P50 should be 20, got " + std::to_string(obj2_p50));

        // Population: Latest values are Obj1: 50.0, Obj2: 20.0
        double pop_p50 = store.get_percentile(obj_combined_feat, 0.5);
        TEST_ASSERT(approx(pop_p50, 20.0) || approx(pop_p50, 50.0), "Population P50 should be 20 or 50, got " + std::to_string(pop_p50));

        // Add 100 to Obj1
        store.update(obj_combined_feat, 1, 100.0);
        // Obj1 history: 10, 50, 100. P50
        obj1_p50 = store.get_percentile(obj_combined_feat, 1, 0.5);
        TEST_ASSERT(approx(obj1_p50, 50.0) || approx(obj1_p50, 100.0), "Obj1 Rolling P50 should be 50 or 100, got " + std::to_string(obj1_p50));

        // Population: Obj1: 100.0, Obj2: 20.0
        pop_p50 = store.get_percentile(obj_combined_feat, 0.5);
        TEST_ASSERT(approx(pop_p50, 20.0) || approx(pop_p50, 100.0), "Population P50 should be 20 or 100, got " + std::to_string(pop_p50));
    });

    return 0;
}
