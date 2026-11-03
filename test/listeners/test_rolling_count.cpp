#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

VULCAN_GROUP(obj, "Test objects");

int main() {
    vulcan_test::run_test("RollingCount - Global", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;

        auto global_decisions = registry.declare<int64_t>("recent_decisions", "Global Tracking", 100);
        registry.declare_group<obj>();
        auto user_decisions   = registry.declare<int64_t, obj>("user_decisions", "Object Tracking", 0);

        // Track last 3 global decisions
        config.add_listeners(global_decisions, {vulcan::listeners::RollingCount(3)});

        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Test Global RollingCount (Window = 3): the initial value 100, then 200, 100
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
        registry.declare_group<obj>();
        auto user_decisions   = registry.declare<int64_t, obj>("user_decisions", "Object Tracking", 0);

        vulcan::store_config config;

        config.add_listeners(user_decisions, {vulcan::listeners::RollingCount(2)});

        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Test Object RollingCount (Window = 2) for Object 5, added with a first value of 555
        fs.add(obj{5}, {{user_decisions, 555}});
        fs.update(user_decisions, obj{5}, 555);
        TEST_ASSERT(fs.get_count(user_decisions, obj{5}, 555) == 2, "Object 5 count of 555 should be 2");
        TEST_ASSERT(fs.contains(user_decisions, obj{5}, 555) == true, "Object 5 should contain 555");

        // Overflow object window
        fs.update(user_decisions, obj{5}, 666); // History is now 555, 666
        TEST_ASSERT(fs.get_count(user_decisions, obj{5}, 555) == 1, "Object 5 count of 555 should drop to 1");
        TEST_ASSERT(fs.get_count(user_decisions, obj{5}, 666) == 1, "Object 5 count of 666 should be 1");
        TEST_ASSERT(fs.contains(user_decisions, obj{5}, 666) == true, "Object 5 should contain 666");
        TEST_ASSERT(fs.contains(user_decisions, obj{5}, 404) == false, "Object 5 should not contain 404");

    });


    vulcan_test::run_test("RollingCount - bool features count trues in the window", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag", true);
        config.add_listeners(flag, {vulcan::listeners::RollingCount(4)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (bool v : {true, false, true, false, false})
            features.update(flag, v);
        // last 4 updates: false, true, false, false
        TEST_ASSERT(features.get_count(flag, true) == 1, "One true in the last 4");
        TEST_ASSERT(features.get_count(flag, false) == 3, "Three falses in the last 4");
        TEST_ASSERT(features.contains(flag, true), "Window contains a true");
    });

    vulcan_test::run_test("RollingCount - bool members are counted separately", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto flag = registry.declare<bool, obj>("flag", "Per-object flag", false);
        config.add_listeners(flag, {vulcan::listeners::RollingCount(2)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.add(obj{1}, {{flag, true}});
        features.update(flag, obj{1}, true);
        features.add(obj{2});   // starts at the declared initial value, false
        TEST_ASSERT(features.get_count(flag, obj{1}, true) == 2, "Object 1 has two trues");
        TEST_ASSERT(features.get_count(flag, obj{2}, true) == 0, "Object 2 has no trues");
    });

    vulcan_test::run_test("RollingCount - int32 features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.declare<int32_t>("n", "Test int32", 7);
        config.add_listeners(n, {vulcan::listeners::RollingCount(3)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {2, 7, 7})
            features.update(n, v);
        TEST_ASSERT(features.get_count(n, 7) == 2, "Two 7s in the last 3");
    });

    vulcan_test::run_test("RollingCount - float features are rejected at construction", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float", 0.0f);
        config.add_listeners(x, {vulcan::listeners::RollingCount(3)});
        TEST_ASSERT(vulcan_test::construction_throws(registry, config), "RollingCount on float should be rejected");
    });

    return 0;
}