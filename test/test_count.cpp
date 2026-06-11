#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    vulcan_test::run_test("RollingCount - Global", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;

        auto global_decisions = registry.global.declare_i64("recent_decisions", "Global Tracking");
        auto user_decisions   = registry.object.declare_i64("user_decisions", "Object Tracking");

        // Track last 3 global decisions
        config.add_listeners(global_decisions, {vulcan::listeners::global::RollingCount(3)});

        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Test Global RollingCount (Window = 3)
        fs.update(global_decisions, 100);
        fs.update(global_decisions, 200);
        fs.update(global_decisions, 100);

        TEST_ASSERT(fs.get_count(global_decisions, 100) == 2, "Global count of 100 should be 2");
        TEST_ASSERT(fs.contains(global_decisions, 100) == true, "Global should contain 100");
        TEST_ASSERT(fs.get_count(global_decisions, 200) == 1, "Global count of 200 should be 1");
        TEST_ASSERT(fs.contains(global_decisions, 200) == true, "Global should contain 200");
        TEST_ASSERT(fs.get_count(global_decisions, 300) == 0, "Global count of 300 should be 0");
        TEST_ASSERT(fs.contains(global_decisions, 300) == false, "Global should not contain 300");

        // Overflow the window (100 -> 200 -> 100 becomes 200 -> 100 -> 300)
        fs.update(global_decisions, 300);
        TEST_ASSERT(fs.get_count(global_decisions, 100) == 1, "Global count of 100 should drop to 1");
        TEST_ASSERT(fs.get_count(global_decisions, 200) == 1, "Global count of 200 should remain 1");
        TEST_ASSERT(fs.get_count(global_decisions, 300) == 1, "Global count of 300 should be 1");
        TEST_ASSERT(fs.contains(global_decisions, 300) == true, "Global should contain 300");
    });

    vulcan_test::run_test("RollingCount - Object", []() {
        vulcan::feature_registry registry;
        auto user_decisions   = registry.object.declare_i64("user_decisions", "Object Tracking");

        vulcan::store_config config;

        config.add_listeners(user_decisions, {vulcan::listeners::object::RollingCount(2)});

        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Test Object RollingCount (Window = 2) for Object 5
        fs.update(user_decisions, 5, 555);
        fs.update(user_decisions, 5, 555);
        TEST_ASSERT(fs.get_count(user_decisions, 5, 555) == 2, "Object 5 count of 555 should be 2");
        TEST_ASSERT(fs.contains(user_decisions, 5, 555) == true, "Object 5 should contain 555");

        // Overflow object window
        fs.update(user_decisions, 5, 666); // History is now 555, 666
        TEST_ASSERT(fs.get_count(user_decisions, 5, 555) == 1, "Object 5 count of 555 should drop to 1");
        TEST_ASSERT(fs.get_count(user_decisions, 5, 666) == 1, "Object 5 count of 666 should be 1");
        TEST_ASSERT(fs.contains(user_decisions, 5, 666) == true, "Object 5 should contain 666");
        TEST_ASSERT(fs.contains(user_decisions, 5, 404) == false, "Object 5 should not contain 404");

    });

    return 0;
}