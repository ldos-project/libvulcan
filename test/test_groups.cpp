#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

VULCAN_GROUP(page, "Test pages");
VULCAN_GROUP(region, "Test regions");

static std::vector<page> members(const vulcan::feature_store& fs) {
    auto all = fs.all<page>();
    return {all.begin(), all.end()};
}

int main() {
    vulcan_test::run_test("Groups - add, remove, contains, count", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        registry.declare_group<region>();
        vulcan::store_config config;
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.add(page{7});
        fs.add(page{8});
        fs.add(page{7});   // already a member
        TEST_ASSERT(fs.count<page>() == 2, "Two distinct members");
        TEST_ASSERT(fs.contains(page{8}) && !fs.contains(page{9}), "contains");
        TEST_ASSERT(fs.count<region>() == 0 && !fs.contains(region{7}), "Groups are independent");

        fs.remove(page{7});
        fs.remove(page{42});   // not a member
        TEST_ASSERT(fs.count<page>() == 1 && !fs.contains(page{7}), "remove");
    });

    vulcan_test::run_test("Groups - all() lists the current members", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        vulcan::store_config config;
        auto fs = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(fs.all<page>().empty(), "No members yet");
        for (int64_t id : {1, 2, 3}) fs.add(page{id});
        TEST_ASSERT((members(fs) == std::vector<page>{page{1}, page{2}, page{3}}), "Members in insertion order");
        fs.remove(page{1});
        TEST_ASSERT((members(fs) == std::vector<page>{page{3}, page{2}}), "Removal moves the last member into the hole");
    });

    vulcan_test::run_test("Groups - remove drops the member's listener state", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        auto hits = registry.declare<double, page>("hits", "Per-page hits");
        vulcan::store_config config;
        config.add_listeners(hits, {vulcan::listeners::RollingWindow(2), vulcan::listeners::Average(),
                                    vulcan::listeners::PopulationPercentile()});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        for (int64_t id : {1, 2, 3}) fs.add(page{id});
        fs.update(hits, page{1}, 10.0);
        fs.update(hits, page{2}, 20.0);
        fs.update(hits, page{3}, 30.0);
        TEST_ASSERT(fs.get_pop_percentile(hits, 1.0) == 30.0, "Max over the population");

        fs.remove(page{3});
        TEST_ASSERT(fs.get_pop_percentile(hits, 1.0) == 20.0, "A removed member leaves the population");
        TEST_ASSERT(fs.get_latest(hits, page{2}) == 20.0, "Other members keep their state");

        fs.add(page{3});
        TEST_ASSERT(fs.get_latest(hits, page{3}) == 0.0 && fs.get_avg(hits, page{3}) == 0.0,
                    "A member added again starts fresh");
    });

    vulcan_test::run_test("Groups - remove leaves other groups and global features alone", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        registry.declare_group<region>();
        auto page_hits = registry.declare<double, page>("page_hits", "Per-page hits");
        auto region_load = registry.declare<double, region>("region_load", "Per-region load");
        auto load = registry.declare<double>("load", "System load");
        vulcan::store_config config;
        config.add_listeners(page_hits, {vulcan::listeners::RollingWindow(1)});
        config.add_listeners(region_load, {vulcan::listeners::RollingWindow(1)});
        config.add_listeners(load, {vulcan::listeners::RollingWindow(1)});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.add(page{1});
        fs.add(region{1});
        fs.update(page_hits, page{1}, 5.0);
        fs.update(region_load, region{1}, 6.0);
        fs.update(load, 7.0);

        fs.remove(page{1});
        TEST_ASSERT(fs.get_latest(page_hits, page{1}) == 0.0, "The page's state is gone");
        TEST_ASSERT(fs.get_latest(region_load, region{1}) == 6.0, "The region with the same id is untouched");
        TEST_ASSERT(fs.get_latest(load) == 7.0, "Global features are untouched");
    });

    vulcan_test::run_test("Groups - rank chooses among the store's current members", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        auto hits = registry.declare<double, page>("hits", "Per-page hits");
        vulcan::store_config store_cfg;
        store_cfg.add_listeners(hits, {vulcan::listeners::RollingWindow(1)});
        vulcan::rank_config<page> config;
        config.set_scoring_fn([&](const vulcan::feature_store& fs, page p) { return fs.get_latest(hits, p); });
        config.set_comparator(vulcan::max);
        auto store = vulcan::make_shared_feature_store(registry, store_cfg);
        auto policy = vulcan::instantiate_rank_policy(registry, config, store);

        TEST_ASSERT(!vulcan::decision(policy), "No members, no decision");
        for (int64_t id : {1, 2}) store->add(page{id});
        store->update(hits, page{1}, 1.0);
        store->update(hits, page{2}, 2.0);
        TEST_ASSERT(*vulcan::decision(policy) == page{2}, "Highest score wins");
        store->remove(page{2});
        TEST_ASSERT(*vulcan::decision(policy) == page{1}, "A removed member is no longer a candidate");
    });

    return 0;
}
