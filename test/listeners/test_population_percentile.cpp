#include <iostream>
#include <cmath>
#include "vulcan.h"
#include "test_utils.hpp"

VULCAN_GROUP(obj, "Test objects");

// Simple approx check
bool approx(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}

int main() {
    vulcan::feature_registry registry;
    vulcan::store_config config;
    
    // 1. Object Feature with Population Distribution
    registry.declare_group<obj>();
    auto obj_feat = registry.declare<double, obj>("o_temp", "Object Temp", 0.0);
    config.add_listeners(obj_feat, {vulcan::listeners::PopulationPercentile()});

    // 2. Object Feature with Both Percentiles
    auto obj_combined_feat = registry.declare<double, obj>("o_comb", "Object Combined", 0.0);
    config.add_listeners(obj_combined_feat, {
        vulcan::listeners::RollingPercentile(100),
        vulcan::listeners::PopulationPercentile()
    });

    auto store = vulcan::instantiate_feature_store(registry, config);

    vulcan_test::run_test("Object Population Distribution", [&]() {
        // Initial state:
        // Obj 1: 10
        // Obj 2: 20
        // Obj 3: 30
        store.add(obj{1}, {{obj_feat, 10.0}});
        store.add(obj{2}, {{obj_feat, 20.0}});
        store.add(obj{3}, {{obj_feat, 30.0}});

        // Median should be 20
        double dist_p50 = store.get_pop_percentile(obj_feat, 0.5);
        TEST_ASSERT(approx(dist_p50, 20.0), "Population P50 should be 20, got " + std::to_string(dist_p50));

        // Update Obj 1: 10 -> 100
        // New state: 20, 30, 100
        // Median should be 30
        store.update(obj_feat, obj{1}, 100.0);
        
        dist_p50 = store.get_pop_percentile(obj_feat, 0.5);
        TEST_ASSERT(approx(dist_p50, 30.0), "Population P50 after update should be 30, got " + std::to_string(dist_p50));

        // Add Obj 4: 5
        // State: 5, 20, 30, 100
        // Median (0.5 * 3 = 1.5 -> idx 1) -> 20
        store.add(obj{4}, {{obj_feat, 5.0}});
        dist_p50 = store.get_pop_percentile(obj_feat, 0.5);
        TEST_ASSERT(approx(dist_p50, 20.0), "Population P50 (4 items) should be 20, got " + std::to_string(dist_p50));
    });

    vulcan_test::run_test("Combined Object Percentiles (Rolling + Population)", [&]() {
        // A store of its own, so the population holds only objects 1 and 2
        auto store = vulcan::instantiate_feature_store(registry, config);
        store.add(obj{1}, {{obj_combined_feat, 10.0}});
        store.add(obj{2}, {{obj_combined_feat, 20.0}});
        store.update(obj_combined_feat, obj{1}, 50.0);

        // Object 1 history [10.0, 50.0], P50 is 10.0 or 50.0
        double obj1_p50 = store.get_temporal_percentile(obj_combined_feat, obj{1}, 0.5);
        TEST_ASSERT(approx(obj1_p50, 10.0) || approx(obj1_p50, 50.0), "Obj1 Rolling P50 should be 10 or 50, got " + std::to_string(obj1_p50));

        // Object 2 history [20.0], P50 is 20.0
        double obj2_p50 = store.get_temporal_percentile(obj_combined_feat, obj{2}, 0.5);
        TEST_ASSERT(approx(obj2_p50, 20.0), "Obj2 Rolling P50 should be 20, got " + std::to_string(obj2_p50));

        // Population: Latest values are Obj1: 50.0, Obj2: 20.0
        double pop_p50 = store.get_pop_percentile(obj_combined_feat, 0.5);
        TEST_ASSERT(approx(pop_p50, 20.0) || approx(pop_p50, 50.0), "Population P50 should be 20 or 50, got " + std::to_string(pop_p50));

        // Add 100 to Obj1
        store.update(obj_combined_feat, obj{1}, 100.0);
        // Obj1 history: 10, 50, 100. P50
        obj1_p50 = store.get_temporal_percentile(obj_combined_feat, obj{1}, 0.5);
        TEST_ASSERT(approx(obj1_p50, 50.0) || approx(obj1_p50, 100.0), "Obj1 Rolling P50 should be 50 or 100, got " + std::to_string(obj1_p50));

        // Population: Obj1: 100.0, Obj2: 20.0
        pop_p50 = store.get_pop_percentile(obj_combined_feat, 0.5);
        TEST_ASSERT(approx(pop_p50, 20.0) || approx(pop_p50, 100.0), "Population P50 should be 20 or 100, got " + std::to_string(pop_p50));
    });


    vulcan_test::run_test("Population percentile of a group with no members is the initial value", []() {
        vulcan::feature_registry registry;
        registry.declare_group<obj>();
        auto hits = registry.declare<double, obj>("hits", "Per-object hits", 3.0);
        vulcan::store_config config;
        config.add_listeners(hits, {vulcan::listeners::PopulationPercentile()});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(fs.get_pop_percentile(hits, 0.5) == 3.0, "No members yet");
        fs.add(obj{1}, {{hits, 10.0}});
        fs.remove(obj{1});
        TEST_ASSERT(fs.get_pop_percentile(hits, 0.5) == 3.0, "Every member removed");
    });


    vulcan_test::run_test("PopulationPercentile - float and int32 features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto x = registry.declare<float, obj>("x", "Per-object float", 0.0f);
        auto n = registry.declare<int32_t, obj>("n", "Per-object int32", 0);
        config.add_listeners(x, {vulcan::listeners::PopulationPercentile()});
        config.add_listeners(n, {vulcan::listeners::PopulationPercentile()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.add(obj{1}, {{x, 1.5f}, {n, 10}});
        features.add(obj{2}, {{x, 3.5f}, {n, 30}});
        features.add(obj{3}, {{x, 2.5f}, {n, 20}});
        TEST_ASSERT(features.get_pop_percentile(x, 0.5) == 2.5, "Median of 1.5, 2.5, 3.5");
        TEST_ASSERT(features.get_pop_percentile(n, 0.5) == 20.0, "Median of 10, 20, 30");
    });

    vulcan_test::run_test("PopulationPercentile - bool features are rejected at construction", []() {
        vulcan::feature_registry registry;
        registry.declare_group<obj>();
        auto o = registry.declare<bool, obj>("o", "Object flag", false);
        vulcan::store_config config;
        config.add_listeners(o, {vulcan::listeners::PopulationPercentile()});
        TEST_ASSERT(vulcan_test::construction_throws(registry, config), "PopulationPercentile on bool should be rejected");
    });

    return 0;
}
