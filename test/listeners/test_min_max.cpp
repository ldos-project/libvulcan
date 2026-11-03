#include "test_utils.hpp"
#include <iostream>

int main() {
    vulcan_test::run_test("MinMax - max of increasing values", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var1 = registry.declare<double>("var1", "Test feature", 0.0);
        config.add_listeners(var1, {vulcan::listeners::MinMax()});
        
        auto features = vulcan::instantiate_feature_store(registry, config);

        for(int i = 0; i < 10; i++) {
            double val = static_cast<double>(i);
            features.update(var1, val);
            double expected_max = val; // Strictly increasing, so max is always current
            TEST_ASSERT(features.get_max(var1) == expected_max, "Max value mismatch");
        }
    });

    vulcan_test::run_test("MinMax - int64 is exact beyond 2^53", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        const int64_t a = 9007199254740993LL;   // 2^53 + 1, not representable as a double
        const int64_t b = 9007199254740995LL;   // 2^53 + 3
        auto n = registry.declare<int64_t>("n", "Large integer", a);
        config.add_listeners(n, {vulcan::listeners::MinMax()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(n, a);
        features.update(n, b);
        TEST_ASSERT(features.get_max(n) == b, "Max is exact");
        TEST_ASSERT(features.get_min(n) == a, "Min is exact");
    });


    vulcan_test::run_test("MinMax - bool features give any-true and all-true", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag", true);
        config.add_listeners(flag, {vulcan::listeners::MinMax()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(flag, true);
        TEST_ASSERT(features.get_max(flag) == true && features.get_min(flag) == true, "All true so far");
        features.update(flag, false);
        TEST_ASSERT(features.get_max(flag) == true, "Max: some value was true");
        TEST_ASSERT(features.get_min(flag) == false, "Min: not every value was true");
    });

    vulcan_test::run_test("MinMax - float features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float", 1.0f);
        config.add_listeners(x, {vulcan::listeners::MinMax()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (float v : {4.0f, 2.0f, 3.0f})
            features.update(x, v);
        TEST_ASSERT(features.get_max(x) == 4.0f, "Max is 4");
        TEST_ASSERT(features.get_min(x) == 1.0f, "Min is 1");
    });

    vulcan_test::run_test("MinMax - int32 features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.declare<int32_t>("n", "Test int32", -5);
        config.add_listeners(n, {vulcan::listeners::MinMax()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {10, 3, 2147483647})
            features.update(n, v);
        TEST_ASSERT(features.get_max(n) == 2147483647, "Max is INT32_MAX");
        TEST_ASSERT(features.get_min(n) == -5, "Min is -5");
    });

    return 0;
}
