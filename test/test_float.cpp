#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

VULCAN_GROUP(obj, "Test objects");

static bool construction_throws(const vulcan::feature_registry& registry, const vulcan::store_config& config) {
    try {
        auto features = vulcan::instantiate_feature_store(registry, config);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    vulcan_test::run_test("Float - RollingWindow returns float values exactly", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float");
        config.add_listeners(x, {vulcan::listeners::RollingWindow(3)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(x, 0.1f);
        features.update(x, 2.5f);
        float latest = features.get_latest(x);
        TEST_ASSERT(latest == 2.5f, "Latest should be 2.5f");
        TEST_ASSERT(features.get_kth_recent(x, 1) == 0.1f, "Previous should be exactly 0.1f");
    });

    vulcan_test::run_test("Float - Statistics listeners", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float");
        config.add_listeners(x, {vulcan::listeners::Average(),
                                 vulcan::listeners::MinMax(),
                                 vulcan::listeners::EWMA({0.5}),
                                 vulcan::listeners::RollingPercentile(4)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (float v : {1.0f, 4.0f, 2.0f, 3.0f})
            features.update(x, v);
        TEST_ASSERT(features.get_avg(x) == 2.5, "Average of 1,4,2,3");
        TEST_ASSERT(features.get_max(x) == 4.0f, "Max is 4");
        TEST_ASSERT(features.get_min(x) == 1.0f, "Min is 1");
        TEST_ASSERT(features.get_temporal_percentile(x, 1.0) == 4.0, "p100 is 4");
        // EWMA(0.5): 1 -> 2.5 -> 2.25 -> 2.625
        TEST_ASSERT(features.get_ewma(x, 0.5) == 2.625, "EWMA of 1,4,2,3 at alpha 0.5");
    });

    vulcan_test::run_test("Float - Object features and population percentile", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto x = registry.declare<float, obj>("x", "Per-object float");
        config.add_listeners(x, {vulcan::listeners::RollingWindow(2), vulcan::listeners::PopulationPercentile()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(x, obj{1}, 1.5f);
        features.update(x, obj{2}, 3.5f);
        features.update(x, obj{3}, 2.5f);
        TEST_ASSERT(features.get_latest(x, obj{2}) == 3.5f, "Object 2 latest");
        TEST_ASSERT(features.get_pop_percentile(x, 0.5) == 2.5, "Median of 1.5, 2.5, 3.5");
    });

    vulcan_test::run_test("Float - RollingCount is rejected at construction", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float");
        config.add_listeners(x, {vulcan::listeners::RollingCount(3)});
        TEST_ASSERT(construction_throws(registry, config), "RollingCount on float should be rejected");
    });

    vulcan_test::run_test("Float - Lookup checks the declared type", []() {
        vulcan::feature_registry registry;
        registry.declare<float>("x", "Test float");
        bool threw = false;
        try {
            registry.lookup<double>("x");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "Lookup as double should fail for a float feature");
    });

    return 0;
}
