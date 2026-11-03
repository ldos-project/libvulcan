#include "test_utils.hpp"
#include <cmath>
#include <iostream>

int main() {
    vulcan_test::run_test("Average - mean of every value", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var1 = registry.declare<double>("var1", "Test feature", 0.0);
        config.add_listeners(var1, {vulcan::listeners::Average()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        double sum = 0;
        for(int i = 0; i < 10; i++) {
            double val = static_cast<double>(i);
            features.update(var1, val);
            sum += val;
            // the initial value 0 is the first of i + 2 values
            double expected_avg = sum / (double)(i + 2);
            double current_avg = features.get_avg(var1);
            TEST_ASSERT(std::abs(current_avg - expected_avg) < 1e-6, "Avg value mismatch: got " + std::to_string(current_avg) + " expected " + std::to_string(expected_avg));
        }
    });

    vulcan_test::run_test("Average - bool features give the fraction of trues", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag", true);
        config.add_listeners(flag, {vulcan::listeners::Average()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (bool v : {false, true, true})
            features.update(flag, v);
        TEST_ASSERT(features.get_avg(flag) == 0.75, "Three of four updates were true");
    });

    vulcan_test::run_test("Average - float features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float", 1.0f);
        config.add_listeners(x, {vulcan::listeners::Average()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (float v : {4.0f, 2.0f, 3.0f})
            features.update(x, v);
        TEST_ASSERT(features.get_avg(x) == 2.5, "Average of 1,4,2,3");
    });

    vulcan_test::run_test("Average - int32 features do not overflow", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.declare<int32_t>("n", "Test int32", -5);
        config.add_listeners(n, {vulcan::listeners::Average()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {10, 3, 2147483647})
            features.update(n, v);
        TEST_ASSERT(features.get_avg(n) == (-5.0 + 10.0 + 3.0 + 2147483647.0) / 4, "Average does not overflow");
    });

    return 0;
}
