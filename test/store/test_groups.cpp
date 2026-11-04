#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>
#include <set>
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

    vulcan_test::run_test("Groups - sample() returns distinct current members", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        vulcan::store_config config;
        auto fs = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(fs.sample<page>(5).empty(), "Sampling an empty group gives nothing");
        for (int64_t id = 0; id < 100; ++id) fs.add(page{id});
        for (std::size_t k : {0, 1, 5, 60, 100, 500}) {
            auto picked = fs.sample<page>(k);
            TEST_ASSERT(picked.size() == std::min<std::size_t>(k, 100), "Sample size is min(k, count)");
            TEST_ASSERT(std::set<page>(picked.begin(), picked.end()).size() == picked.size(), "No duplicates");
            for (page p : picked) TEST_ASSERT(fs.contains(p), "Every sampled page is a member");
        }
    });

    vulcan_test::run_test("Groups - sample() can pick every member", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        vulcan::store_config config;
        auto fs = vulcan::instantiate_feature_store(registry, config);

        for (int64_t id = 0; id < 10; ++id) fs.add(page{id});
        std::set<page> seen;
        for (int i = 0; i < 1000; ++i) seen.insert(fs.sample<page>(1)[0]);
        TEST_ASSERT(seen.size() == 10, "1000 draws of one member reach all 10");
    });

    vulcan_test::run_test("Groups - remove drops the member's listener state", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        auto hits = registry.declare<double, page>("hits", "Per-page hits", 0.0);
        vulcan::store_config config;
        config.add_listener(hits, vulcan::listeners::RollingWindow(2))
            .add_listener(hits, vulcan::listeners::Average())
            .add_listener(hits, vulcan::listeners::PopulationPercentile());
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
                    "A member added again starts at its initial value");
    });

    vulcan_test::run_test("Groups - remove leaves other groups and global features alone", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        registry.declare_group<region>();
        auto page_hits = registry.declare<double, page>("page_hits", "Per-page hits", 0.0);
        auto region_load = registry.declare<double, region>("region_load", "Per-region load", 0.0);
        auto load = registry.declare<double>("load", "System load", 0.0);
        vulcan::store_config config;
        config.add_listener(page_hits, vulcan::listeners::RollingWindow(1));
        config.add_listener(region_load, vulcan::listeners::RollingWindow(1));
        config.add_listener(load, vulcan::listeners::RollingWindow(1));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.add(page{1});
        fs.add(region{1});
        fs.update(page_hits, page{1}, 5.0);
        fs.update(region_load, region{1}, 6.0);
        fs.update(load, 7.0);

        fs.remove(page{1});
        fs.add(page{1});
        TEST_ASSERT(fs.get_latest(page_hits, page{1}) == 0.0, "The page's state is gone");
        TEST_ASSERT(fs.get_latest(region_load, region{1}) == 6.0, "The region with the same id is untouched");
        TEST_ASSERT(fs.get_latest(load) == 7.0, "Global features are untouched");
    });

    vulcan_test::run_test("Non-members - reads and updates throw", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        auto hits = registry.declare<double, page>("hits", "Per-page hits", 0.0);
        auto unwatched = registry.declare<double, page>("unwatched", "A feature with no listeners", 0.0);
        vulcan::store_config config;
        config.add_listener(hits, vulcan::listeners::RollingWindow(1));
        auto fs = vulcan::instantiate_feature_store(registry, config);
        fs.add(page{1});

        auto throws = [](auto f) {
            try { f(); } catch (const std::logic_error&) { return true; }
            return false;
        };
        TEST_ASSERT(throws([&] { fs.update(hits, page{2}, 1.0); }), "Update of a never-added page");
        TEST_ASSERT(throws([&] { fs.get_latest(hits, page{2}); }), "Read of a never-added page");
        TEST_ASSERT(throws([&] { fs.update(unwatched, page{2}, 1.0); }), "Update of a feature with no listeners");
        fs.remove(page{1});
        TEST_ASSERT(throws([&] { fs.update(hits, page{1}, 1.0); }), "Update of a removed page");
        TEST_ASSERT(throws([&] { fs.get_latest(hits, page{1}); }), "Read of a removed page");
    });

    return 0;
}
