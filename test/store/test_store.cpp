#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    vulcan_test::run_test("Store - a getter on a feature with no listeners throws", []() {
        vulcan::feature_registry registry;
        auto n = registry.declare<int64_t>("n", "Feature with no listeners", 0);
        vulcan::store_config config;
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(n, 5);   // an update with no listeners is a no-op
        bool threw = false;
        try {
            features.get_max(n);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "Reading a feature with no listeners should throw std::runtime_error");
    });

    vulcan_test::run_test("Store - two listeners of the same kind on one feature are rejected", []() {
        vulcan::feature_registry registry;
        auto x = registry.declare<double>("x", "Test feature", 0.0);
        vulcan::store_config config;
        config.add_listener(x, vulcan::listeners::RollingWindow(3))
            .add_listener(x, vulcan::listeners::RollingWindow(5));   // same kind twice
        bool threw = false;
        try {
            auto features = vulcan::instantiate_feature_store(registry, config);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "A duplicate listener should throw std::runtime_error when the store is built");
    });

    return 0;
}
