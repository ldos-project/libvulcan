#include "test_utils.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

VULCAN_GROUP(obj, "Test objects");

int main() {
    vulcan_test::run_test("EWMA - Global initialization", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.declare<double>("g_val", "Global value", 10.0);
        config.add_listener(var, vulcan::listeners::EWMA({0.1, 0.5}));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Before any update, EWMA equals the initial value exactly
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.1) - 10.0) < 1e-9, "Global alpha=0.1 init should be 10.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.5) - 10.0) < 1e-9, "Global alpha=0.5 init should be 10.0");
    });

    vulcan_test::run_test("EWMA - Global multi-alpha correctness", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.declare<double>("g_val", "Global value", 10.0);
        config.add_listener(var, vulcan::listeners::EWMA({0.1, 0.5}));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Start at the initial value 10, then feed 20 and 30
        fs.update(var, 20.0);
        // alpha=0.1: 0.1*20 + 0.9*10 = 11.0
        // alpha=0.5: 0.5*20 + 0.5*10 = 15.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.1) - 11.0) < 1e-9, "Global alpha=0.1 after [10,20] should be 11.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.5) - 15.0) < 1e-9, "Global alpha=0.5 after [10,20] should be 15.0");

        fs.update(var, 30.0);
        // alpha=0.1: 0.1*30 + 0.9*11 = 3 + 9.9 = 12.9
        // alpha=0.5: 0.5*30 + 0.5*15 = 15 + 7.5 = 22.5
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.1) - 12.9) < 1e-9, "Global alpha=0.1 after [10,20,30] should be 12.9");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.5) - 22.5) < 1e-9, "Global alpha=0.5 after [10,20,30] should be 22.5");
    });

    vulcan_test::run_test("EWMA - Global int64_t update path", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.declare<int64_t>("g_int", "Global int", 4);
        config.add_listener(var, vulcan::listeners::EWMA({0.5}));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.update(var, (int64_t)8);
        // alpha=0.5: 0.5*8 + 0.5*4 = 6.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.5) - 6.0) < 1e-9, "Global int path alpha=0.5 after [4,8] should be 6.0");
    });

    vulcan_test::run_test("EWMA - Object initialization and isolation", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto var = registry.declare<double, obj>("obj_val", "Per-object value", 0.0);
        config.add_listener(var, vulcan::listeners::EWMA({0.2, 0.8}));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        bool threw = false;
        try {
            fs.get_ewma(var, obj{99}, 0.2);
        } catch (const std::logic_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "Reading a non-member should throw");

        fs.add(obj{1}, {{var, 5.0}});
        fs.add(obj{2}, {{var, 100.0}});

        // EWMA starts at each member's first value
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{1}, 0.2) - 5.0) < 1e-9, "obj1 alpha=0.2 init should be 5.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{1}, 0.8) - 5.0) < 1e-9, "obj1 alpha=0.8 init should be 5.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{2}, 0.2) - 100.0) < 1e-9, "obj2 alpha=0.2 init should be 100.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{2}, 0.8) - 100.0) < 1e-9, "obj2 alpha=0.8 init should be 100.0");
    });

    vulcan_test::run_test("EWMA - Object multi-alpha correctness", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto var = registry.declare<double, obj>("obj_val", "Per-object value", 0.0);
        config.add_listener(var, vulcan::listeners::EWMA({0.2, 0.8}));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.add(obj{1}, {{var, 5.0}});
        fs.add(obj{2}, {{var, 100.0}});
        fs.update(var, obj{1}, 10.0);
        // obj1 alpha=0.2: 0.2*10 + 0.8*5 = 2 + 4 = 6.0
        // obj1 alpha=0.8: 0.8*10 + 0.2*5 = 8 + 1 = 9.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{1}, 0.2) - 6.0) < 1e-9, "obj1 alpha=0.2 after [5,10] should be 6.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{1}, 0.8) - 9.0) < 1e-9, "obj1 alpha=0.8 after [5,10] should be 9.0");

        fs.update(var, obj{2}, 200.0);
        // obj2 alpha=0.2: 0.2*200 + 0.8*100 = 40 + 80 = 120.0
        // obj2 alpha=0.8: 0.8*200 + 0.2*100 = 160 + 20 = 180.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{2}, 0.2) - 120.0) < 1e-9, "obj2 alpha=0.2 after [100,200] should be 120.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{2}, 0.8) - 180.0) < 1e-9, "obj2 alpha=0.8 after [100,200] should be 180.0");

        // obj1 must be unchanged by obj2 updates
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{1}, 0.2) - 6.0) < 1e-9, "obj1 should be unaffected by obj2 updates");
    });

    vulcan_test::run_test("EWMA - Object int64_t update path", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto var = registry.declare<int64_t, obj>("obj_int", "Per-object int", 0);
        config.add_listener(var, vulcan::listeners::EWMA({0.5}));
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.add(obj{7}, {{var, 4}});
        fs.update(var, obj{7}, (int64_t)8);
        // alpha=0.5: 0.5*8 + 0.5*4 = 6.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, obj{7}, 0.5) - 6.0) < 1e-9, "Object int path alpha=0.5 after [4,8] should be 6.0");
    });


    vulcan_test::run_test("EWMA - bool features give a decayed fraction of trues", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag", true);
        config.add_listener(flag, vulcan::listeners::EWMA({0.5}));
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (bool v : {false, true, true})
            features.update(flag, v);
        // EWMA(0.5): 1 -> 0.5 -> 0.75 -> 0.875
        TEST_ASSERT(features.get_ewma(flag, 0.5) == 0.875, "EWMA of 1,0,1,1 at alpha 0.5");
    });

    vulcan_test::run_test("EWMA - float features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float", 1.0f);
        config.add_listener(x, vulcan::listeners::EWMA({0.5}));
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (float v : {4.0f, 2.0f, 3.0f})
            features.update(x, v);
        // EWMA(0.5): 1 -> 2.5 -> 2.25 -> 2.625
        TEST_ASSERT(features.get_ewma(x, 0.5) == 2.625, "EWMA of 1,4,2,3 at alpha 0.5");
    });

    return 0;
}
