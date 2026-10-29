#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    vulcan_test::run_test("Int32 - RollingWindow and RollingCount", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.global.declare<int32_t>("n", "Test int32");
        config.add_listeners(n, {vulcan::listeners::RollingWindow(3), vulcan::listeners::RollingCount(3)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {7, 2, 7, 7})
            features.update(n, v);
        int32_t latest = features.get_latest(n);
        TEST_ASSERT(latest == 7, "Latest is 7");
        TEST_ASSERT(features.get_kth_recent(n, 2) == 2, "Two back is 2");
        TEST_ASSERT(features.get_count(n, 7) == 2, "Two 7s in the last 3");
    });

    vulcan_test::run_test("Int32 - Statistics listeners", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.global.declare<int32_t>("n", "Test int32");
        config.add_listeners(n, {vulcan::listeners::Average(),
                                 vulcan::listeners::MinMax(),
                                 vulcan::listeners::RollingPercentile(4)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {-5, 10, 3, 2147483647})
            features.update(n, v);
        TEST_ASSERT(features.get_max(n) == 2147483647, "Max is INT32_MAX");
        TEST_ASSERT(features.get_min(n) == -5, "Min is -5");
        TEST_ASSERT(features.get_avg(n) == (-5.0 + 10.0 + 3.0 + 2147483647.0) / 4, "Average does not overflow");
        TEST_ASSERT(features.get_temporal_percentile(n, 0.0) == -5.0, "p0 is -5");
    });

    vulcan_test::run_test("Int32 - Object features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.object.declare<int32_t>("n", "Per-object int32");
        config.add_listeners(n, {vulcan::listeners::RollingWindow(2), vulcan::listeners::PopulationPercentile()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(n, 1, 10);
        features.update(n, 2, 30);
        features.update(n, 3, 20);
        TEST_ASSERT(features.get_latest(n, 2) == 30, "Object 2 latest");
        TEST_ASSERT(features.get_pop_percentile(n, 0.5) == 20.0, "Median of 10, 20, 30");
    });

    vulcan_test::run_test("Int32 - Lookup checks the declared type", []() {
        vulcan::feature_registry registry;
        registry.global.declare<int32_t>("n", "Test int32");
        bool threw = false;
        try {
            registry.global.lookup<int64_t>("n");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "Lookup as int64_t should fail for an int32 feature");
    });

    return 0;
}
