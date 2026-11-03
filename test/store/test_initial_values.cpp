#include "test_utils.hpp"
#include <iostream>

VULCAN_GROUP(page, "Test pages");

int main() {
    vulcan_test::run_test("Initial values - add records each feature's initial or given value", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        auto hits = registry.declare<int64_t, page>("hits", "Per-page hits", 0);
        auto size = registry.declare<double, page>("size", "Page size in bytes", -1.0);
        vulcan::store_config config;
        config.add_listener(hits, vulcan::listeners::RollingWindow(1));
        config.add_listener(size, vulcan::listeners::RollingWindow(1))
            .add_listener(size, vulcan::listeners::Average());
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.add(page{1});
        fs.add(page{2}, {{size, 4096.0}});
        TEST_ASSERT(fs.get_latest(hits, page{1}) == 0 && fs.get_latest(size, page{1}) == -1.0, "Declared initial values");
        TEST_ASSERT(fs.get_latest(hits, page{2}) == 0 && fs.get_latest(size, page{2}) == 4096.0, "A given value replaces the initial value");

        fs.update(size, page{2}, 8192.0);
        TEST_ASSERT(fs.get_avg(size, page{2}) == 6144.0, "The first value is a real sample");

        fs.add(page{2}, {{size, 1.0}});   // already a member
        TEST_ASSERT(fs.get_latest(size, page{2}) == 8192.0, "Adding a member again changes nothing");
    });

    vulcan_test::run_test("Initial values - global features start at their initial value", []() {
        vulcan::feature_registry registry;
        auto load = registry.declare<double>("load", "System load", 0.5);
        vulcan::store_config config;
        config.add_listener(load, vulcan::listeners::RollingWindow(2))
            .add_listener(load, vulcan::listeners::MinMax());
        auto fs = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(fs.get_latest(load) == 0.5 && fs.get_max(load) == 0.5, "Read before any update");
        fs.update(load, 0.9);
        TEST_ASSERT(fs.get_kth_recent(load, 1) == 0.5 && fs.get_min(load) == 0.5, "The initial value is the first sample");
    });

    return 0;
}
