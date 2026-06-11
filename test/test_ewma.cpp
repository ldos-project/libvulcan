#include "test_utils.hpp"
#include <cmath>
#include <iostream>

int main() {
    vulcan_test::run_test("EWMA - Global initialization", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.global.declare_f64("g_val", "Global value");
        config.add_listeners(var, {vulcan::listeners::global::EWMA({0.1, 0.5})});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        // First update: EWMA should equal the seeded value exactly
        fs.update(var, 10.0);
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.1) - 10.0) < 1e-9, "Global alpha=0.1 init should be 10.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.5) - 10.0) < 1e-9, "Global alpha=0.5 init should be 10.0");
    });

    vulcan_test::run_test("EWMA - Global multi-alpha correctness", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.global.declare_f64("g_val", "Global value");
        config.add_listeners(var, {vulcan::listeners::global::EWMA({0.1, 0.5})});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Seed with 10, then feed 20 and 30
        fs.update(var, 10.0);
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
        auto var = registry.global.declare_i64("g_int", "Global int");
        config.add_listeners(var, {vulcan::listeners::global::EWMA({0.5})});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.update(var, (int64_t)4);
        fs.update(var, (int64_t)8);
        // alpha=0.5: 0.5*8 + 0.5*4 = 6.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 0.5) - 6.0) < 1e-9, "Global int path alpha=0.5 after [4,8] should be 6.0");
    });

    vulcan_test::run_test("EWMA - Object initialization and isolation", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.object.declare_f64("obj_val", "Per-object value");
        config.add_listeners(var, {vulcan::listeners::object::EWMA({0.2, 0.8})});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        // Unseen object should return 0.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 99, 0.2)) < 1e-9, "Unseen object should return 0.0");

        // Seed both objects
        fs.update(var, (int64_t)1, 5.0);
        fs.update(var, (int64_t)2, 100.0);

        // First update is the seeded value
        TEST_ASSERT(std::abs(fs.get_ewma(var, 1, 0.2) - 5.0) < 1e-9, "obj1 alpha=0.2 init should be 5.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 1, 0.8) - 5.0) < 1e-9, "obj1 alpha=0.8 init should be 5.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 2, 0.2) - 100.0) < 1e-9, "obj2 alpha=0.2 init should be 100.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 2, 0.8) - 100.0) < 1e-9, "obj2 alpha=0.8 init should be 100.0");
    });

    vulcan_test::run_test("EWMA - Object multi-alpha correctness", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.object.declare_f64("obj_val", "Per-object value");
        config.add_listeners(var, {vulcan::listeners::object::EWMA({0.2, 0.8})});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.update(var, (int64_t)1, 5.0);
        fs.update(var, (int64_t)1, 10.0);
        // obj1 alpha=0.2: 0.2*10 + 0.8*5 = 2 + 4 = 6.0
        // obj1 alpha=0.8: 0.8*10 + 0.2*5 = 8 + 1 = 9.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 1, 0.2) - 6.0) < 1e-9, "obj1 alpha=0.2 after [5,10] should be 6.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 1, 0.8) - 9.0) < 1e-9, "obj1 alpha=0.8 after [5,10] should be 9.0");

        fs.update(var, (int64_t)2, 100.0);
        fs.update(var, (int64_t)2, 200.0);
        // obj2 alpha=0.2: 0.2*200 + 0.8*100 = 40 + 80 = 120.0
        // obj2 alpha=0.8: 0.8*200 + 0.2*100 = 160 + 20 = 180.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 2, 0.2) - 120.0) < 1e-9, "obj2 alpha=0.2 after [100,200] should be 120.0");
        TEST_ASSERT(std::abs(fs.get_ewma(var, 2, 0.8) - 180.0) < 1e-9, "obj2 alpha=0.8 after [100,200] should be 180.0");

        // obj1 must be unchanged by obj2 updates
        TEST_ASSERT(std::abs(fs.get_ewma(var, 1, 0.2) - 6.0) < 1e-9, "obj1 should be unaffected by obj2 updates");
    });

    vulcan_test::run_test("EWMA - Object int64_t update path", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var = registry.object.declare_i64("obj_int", "Per-object int");
        config.add_listeners(var, {vulcan::listeners::object::EWMA({0.5})});
        auto fs = vulcan::instantiate_feature_store(registry, config);

        fs.update(var, (int64_t)7, (int64_t)4);
        fs.update(var, (int64_t)7, (int64_t)8);
        // alpha=0.5: 0.5*8 + 0.5*4 = 6.0
        TEST_ASSERT(std::abs(fs.get_ewma(var, 7, 0.5) - 6.0) < 1e-9, "Object int path alpha=0.5 after [4,8] should be 6.0");
    });

    return 0;
}
