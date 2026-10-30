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
    vulcan_test::run_test("Bool - Global RollingWindow returns bool", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag");
        config.add_listeners(flag, {vulcan::listeners::RollingWindow(3)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(features.get_latest(flag) == false, "Unseen flag should read false");
        features.update(flag, true);
        features.update(flag, false);
        features.update(flag, true);
        bool latest = features.get_latest(flag);
        TEST_ASSERT(latest == true, "Latest should be true");
        TEST_ASSERT(features.get_kth_recent(flag, 1) == false, "Previous should be false");
        TEST_ASSERT(features.get_kth_recent(flag, 2) == true, "Oldest should be true");
        TEST_ASSERT(features.get_kth_recent(flag, 3) == false, "Out of window should read false");
    });

    vulcan_test::run_test("Bool - Global RollingCount counts trues in the window", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag");
        config.add_listeners(flag, {vulcan::listeners::RollingCount(4)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (bool v : {true, true, false, true, false, false})
            features.update(flag, v);
        // last 4 updates: false, true, false, false
        TEST_ASSERT(features.get_count(flag, true) == 1, "One true in the last 4");
        TEST_ASSERT(features.get_count(flag, false) == 3, "Three falses in the last 4");
        TEST_ASSERT(features.contains(flag, true), "Window contains a true");
    });

    vulcan_test::run_test("Bool - Average and EWMA give the fraction of trues", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag");
        config.add_listeners(flag, {vulcan::listeners::Average(), vulcan::listeners::EWMA({0.5})});
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (bool v : {true, false, true, true})
            features.update(flag, v);
        TEST_ASSERT(features.get_avg(flag) == 0.75, "Three of four updates were true");
        // EWMA(0.5): 1 -> 0.5 -> 0.75 -> 0.875
        TEST_ASSERT(features.get_ewma(flag, 0.5) == 0.875, "EWMA of 1,0,1,1 at alpha 0.5");
    });

    vulcan_test::run_test("Bool - Object features are isolated per object", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto flag = registry.declare<bool, obj>("flag", "Per-object flag");
        config.add_listeners(flag, {vulcan::listeners::RollingWindow(2), vulcan::listeners::RollingCount(2)});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(flag, obj{1}, true);
        features.update(flag, obj{1}, true);
        features.update(flag, obj{2}, false);
        TEST_ASSERT(features.get_latest(flag, obj{1}) == true, "Object 1 latest is true");
        TEST_ASSERT(features.get_latest(flag, obj{2}) == false, "Object 2 latest is false");
        TEST_ASSERT(features.get_count(flag, obj{1}, true) == 2, "Object 1 has two trues");
        TEST_ASSERT(features.get_count(flag, obj{2}, true) == 0, "Object 2 has no trues");
    });

    vulcan_test::run_test("Bool - MinMax gives any-true and all-true", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag");
        config.add_listeners(flag, {vulcan::listeners::MinMax()});
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(flag, true);
        features.update(flag, true);
        TEST_ASSERT(features.get_max(flag) == true && features.get_min(flag) == true, "All true so far");
        features.update(flag, false);
        TEST_ASSERT(features.get_max(flag) == true, "Max: some value was true");
        TEST_ASSERT(features.get_min(flag) == false, "Min: not every value was true");
    });

    vulcan_test::run_test("Bool - Percentiles are rejected at construction", []() {
        vulcan::feature_registry registry;
        auto g = registry.declare<bool>("g", "Global flag");
        registry.declare_group<obj>();
        auto o = registry.declare<bool, obj>("o", "Object flag");

        vulcan::store_config percentile;
        percentile.add_listeners(g, {vulcan::listeners::RollingPercentile(4)});
        TEST_ASSERT(construction_throws(registry, percentile), "RollingPercentile on bool should be rejected");

        vulcan::store_config population;
        population.add_listeners(o, {vulcan::listeners::PopulationPercentile()});
        TEST_ASSERT(construction_throws(registry, population), "PopulationPercentile on bool should be rejected");
    });

    vulcan_test::run_test("Bool - Lookup checks the declared type", []() {
        vulcan::feature_registry registry;
        registry.declare<bool>("flag", "Test flag");
        auto again = registry.lookup<bool>("flag");
        TEST_ASSERT(again.name == "flag", "Lookup as bool succeeds");

        bool threw = false;
        try {
            registry.lookup<int64_t>("flag");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "Lookup as int64_t should fail for a bool feature");
    });

    return 0;
}
