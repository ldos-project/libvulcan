#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

VULCAN_GROUP(obj, "Test objects");

int main() {
    vulcan_test::run_test("RollingWindow - Global", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var1 = registry.declare<double>("var1", "Test feature", 0.0);
        config.add_listener(var1, vulcan::listeners::RollingWindow(5));

        auto features = vulcan::instantiate_feature_store(registry, config);
        
        try {
            for(int i=1; i<=300; i++) {
                features.update(var1, 10 * i);
                if (i > 3) {
                    TEST_ASSERT(features.get_latest(var1) == i * 10, "Incorrect value");
                    TEST_ASSERT(features.get_kth_recent(var1, 1) == (i - 1) * 10, "Incorrect value");
                    TEST_ASSERT(features.get_kth_recent(var1, 2) == (i - 2) * 10, "Incorrect value");
                }
            }            
        } catch (const std::exception& e) {
            TEST_ASSERT(false, std::string("Unexpected exception: ") + e.what());
        }
    });

    vulcan_test::run_test("RollingWindow - Object", []() {
        vulcan::feature_registry registry;
        
        // Declare object features
        registry.declare_group<obj>();
        auto obj_val = registry.declare<double, obj>("obj_val", "Object specific value", 0.0);
        
        // Add listeners
        vulcan::store_config config;
        config.add_listener(obj_val, vulcan::listeners::RollingWindow(3));
        
        auto store = vulcan::instantiate_feature_store(registry, config);
        store.add(obj{1});
        store.add(obj{2});
        
        for(int i=0; i<100; i++){
            store.update(obj_val, obj{1}, 10.0 * i);
            store.update(obj_val, obj{2}, 7.0 * i);

            if (i > 3) {
                TEST_ASSERT(store.get_latest(obj_val, obj{1}) == i * 10, "[latest] Incorrect value for obj 1");
                TEST_ASSERT(store.get_latest(obj_val, obj{2}) == i * 7, "[latest] Incorrect value for obj 2");
                TEST_ASSERT(store.get_kth_recent(obj_val, obj{1}, 1) == (i - 1) * 10, "[kth] Incorrect value for obj 1");
                TEST_ASSERT(store.get_kth_recent(obj_val, obj{2}, 1) == (i - 1) * 7, "[kth] Incorrect value for obj 2");
                TEST_ASSERT(store.get_kth_recent(obj_val, obj{1}, 2) == (i - 2) * 10, "[kth] Incorrect value for obj 1");
                TEST_ASSERT(store.get_kth_recent(obj_val, obj{2}, 2) == (i - 2) * 7, "[kth] Incorrect value for obj 2");
            }
        }
    });

    vulcan_test::run_test("RollingWindow - Global i64", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto var1 = registry.declare<int64_t>("var1_i64", "Test i64 feature", 0);
        config.add_listener(var1, vulcan::listeners::RollingWindow(5));

        auto features = vulcan::instantiate_feature_store(registry, config);

        for(int i=1; i<=300; i++) {
            features.update(var1, static_cast<int64_t>(10 * i));
            if (i > 3) {
                TEST_ASSERT(features.get_latest(var1) == i * 10, "get_latest i64 incorrect");
                TEST_ASSERT(features.get_kth_recent(var1, 1) == (i - 1) * 10, "get_kth_recent i64 incorrect");
                TEST_ASSERT(features.get_kth_recent(var1, 2) == (i - 2) * 10, "get_kth_recent i64 k=2 incorrect");
            }
        }
    });

    vulcan_test::run_test("RollingWindow - Global i64 large values", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        int64_t large = (1LL << 53) + 1;
        auto var1 = registry.declare<int64_t>("big_i64", "Large i64 feature", large);
        config.add_listener(var1, vulcan::listeners::RollingWindow(3));

        auto features = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(features.get_latest(var1) == large, "Large i64 value lost precision");

        features.update(var1, large + 1);
        TEST_ASSERT(features.get_latest(var1) == large + 1, "Large i64 latest incorrect");
        TEST_ASSERT(features.get_kth_recent(var1, 1) == large, "Large i64 kth incorrect");
    });

    vulcan_test::run_test("RollingWindow - Object i64", []() {
        vulcan::feature_registry registry;
        registry.declare_group<obj>();
        auto obj_val = registry.declare<int64_t, obj>("obj_i64", "Object i64 feature", 0);

        vulcan::store_config config;
        config.add_listener(obj_val, vulcan::listeners::RollingWindow(3));

        auto store = vulcan::instantiate_feature_store(registry, config);
        store.add(obj{1});
        store.add(obj{2});

        for(int i=0; i<100; i++){
            store.update(obj_val, obj{1}, static_cast<int64_t>(10 * i));
            store.update(obj_val, obj{2}, static_cast<int64_t>(7 * i));

            if (i > 3) {
                TEST_ASSERT(store.get_latest(obj_val, obj{1}) == i * 10, "[latest] Object i64 incorrect for obj 1");
                TEST_ASSERT(store.get_latest(obj_val, obj{2}) == i * 7, "[latest] Object i64 incorrect for obj 2");
                TEST_ASSERT(store.get_kth_recent(obj_val, obj{1}, 1) == (i - 1) * 10, "[kth] Object i64 incorrect for obj 1");
                TEST_ASSERT(store.get_kth_recent(obj_val, obj{2}, 1) == (i - 1) * 7, "[kth] Object i64 incorrect for obj 2");
            }
        }
    });

    vulcan_test::run_test("RollingWindow - Object window=0 (unbounded)", []() {
        vulcan::feature_registry registry;
        registry.declare_group<obj>();
        auto obj_val = registry.declare<double, obj>("obj_f64_w0", "Object f64 unbounded window", 0.0);
        auto obj_i64 = registry.declare<int64_t, obj>("obj_i64_w0", "Object i64 unbounded window", 0);

        vulcan::store_config config;
        config.add_listener(obj_val, vulcan::listeners::RollingWindow(0));
        config.add_listener(obj_i64, vulcan::listeners::RollingWindow(0));

        auto store = vulcan::instantiate_feature_store(registry, config);
        store.add(obj{1});

        for (int i = 1; i <= 20; i++) {
            store.update(obj_val, obj{1}, 10.0 * i);
            store.update(obj_i64, obj{1}, static_cast<int64_t>(10 * i));
        }

        TEST_ASSERT(store.get_latest(obj_val, obj{1}) == 200.0, "window=0 f64 latest incorrect");
        TEST_ASSERT(store.get_latest(obj_i64, obj{1}) == 200, "window=0 i64 latest incorrect");
        TEST_ASSERT(store.get_kth_recent(obj_val, obj{1}, 1) == 190.0, "window=0 f64 kth incorrect");
        TEST_ASSERT(store.get_kth_recent(obj_i64, obj{1}, 1) == 190, "window=0 i64 kth incorrect");
    });


    vulcan_test::run_test("RollingWindow - get_window_avg averages only the window", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto g = registry.declare<double>("g", "Global value", 1.0);
        registry.declare_group<obj>();
        auto o = registry.declare<double, obj>("o", "Object value", 1.0);
        auto both = registry.declare<double>("both", "Average and RollingWindow", 1.0);
        config.add_listener(g, vulcan::listeners::RollingWindow(3));
        config.add_listener(o, vulcan::listeners::RollingWindow(2));
        config.add_listener(both, vulcan::listeners::Average())
            .add_listener(both, vulcan::listeners::RollingWindow(2));
        auto features = vulcan::instantiate_feature_store(registry, config);
        features.add(obj{7});

        // Each feature starts at its initial value 1, then gets 2, 3, 10
        for (double v : {2.0, 3.0, 10.0}) {
            features.update(g, v);
            features.update(o, obj{7}, v);
            features.update(both, v);
        }
        TEST_ASSERT(features.get_window_avg(g) == 5.0, "Mean of the last 3: 2, 3, 10");
        TEST_ASSERT(features.get_window_avg(o, obj{7}) == 6.5, "Mean of the last 2: 3, 10");

        // With both listeners attached, the two getters are different statistics.
        TEST_ASSERT(features.get_avg(both) == 4.0, "All-time mean of 1, 2, 3, 10");
        TEST_ASSERT(features.get_window_avg(both) == 6.5, "Window mean of 3, 10");

        // get_avg reads only Average; it does not fall back to a RollingWindow.
        bool threw = false;
        try {
            features.get_avg(g);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        TEST_ASSERT(threw, "get_avg without an Average listener should throw");
    });


    vulcan_test::run_test("RollingWindow - get_kth_recent past the history gives the oldest value", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto partial = registry.declare<double>("partial", "Window not yet full", 1.0);
        auto full = registry.declare<double>("full", "Window full", 1.0);
        config.add_listener(partial, vulcan::listeners::RollingWindow(5));
        config.add_listener(full, vulcan::listeners::RollingWindow(2));
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (double v : {2.0, 3.0}) {
            features.update(partial, v);
            features.update(full, v);
        }
        TEST_ASSERT(features.get_kth_recent(partial, 2) == 1.0 && features.get_kth_recent(partial, 9) == 1.0,
                    "History 1, 2, 3: k = 2 and beyond give 1");
        TEST_ASSERT(features.get_kth_recent(full, 1) == 2.0 && features.get_kth_recent(full, 9) == 2.0,
                    "Window holds 2, 3: k = 1 and beyond give 2");
    });


    vulcan_test::run_test("RollingWindow - bool features return bool", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto flag = registry.declare<bool>("flag", "Test flag", false);
        config.add_listener(flag, vulcan::listeners::RollingWindow(3));
        auto features = vulcan::instantiate_feature_store(registry, config);

        TEST_ASSERT(features.get_latest(flag) == false, "Flag starts at its initial value");
        features.update(flag, true);
        features.update(flag, false);
        features.update(flag, true);
        bool latest = features.get_latest(flag);
        TEST_ASSERT(latest == true, "Latest should be true");
        TEST_ASSERT(features.get_kth_recent(flag, 1) == false, "Previous should be false");
        TEST_ASSERT(features.get_kth_recent(flag, 2) == true, "Oldest should be true");
    });

    vulcan_test::run_test("RollingWindow - float features return float values exactly", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto x = registry.declare<float>("x", "Test float", 0.1f);
        config.add_listener(x, vulcan::listeners::RollingWindow(3));
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.update(x, 2.5f);
        float latest = features.get_latest(x);
        TEST_ASSERT(latest == 2.5f, "Latest should be 2.5f");
        TEST_ASSERT(features.get_kth_recent(x, 1) == 0.1f, "Previous should be exactly 0.1f");
    });

    vulcan_test::run_test("RollingWindow - int32 features", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        auto n = registry.declare<int32_t>("n", "Test int32", 7);
        config.add_listener(n, vulcan::listeners::RollingWindow(3));
        auto features = vulcan::instantiate_feature_store(registry, config);

        for (int32_t v : {2, 7, 7})
            features.update(n, v);
        int32_t latest = features.get_latest(n);
        TEST_ASSERT(latest == 7, "Latest is 7");
        TEST_ASSERT(features.get_kth_recent(n, 2) == 2, "Two back is 2");
    });

    vulcan_test::run_test("RollingWindow - bool, float and int32 members keep separate windows", []() {
        vulcan::feature_registry registry;
        vulcan::store_config config;
        registry.declare_group<obj>();
        auto flag = registry.declare<bool, obj>("flag", "Per-object flag", false);
        auto x = registry.declare<float, obj>("x", "Per-object float", 0.0f);
        auto n = registry.declare<int32_t, obj>("n", "Per-object int32", 0);
        config.add_listener(flag, vulcan::listeners::RollingWindow(2));
        config.add_listener(x, vulcan::listeners::RollingWindow(2));
        config.add_listener(n, vulcan::listeners::RollingWindow(2));
        auto features = vulcan::instantiate_feature_store(registry, config);

        features.add(obj{1}, {{flag, true}, {x, 1.5f}, {n, 10}});
        features.update(flag, obj{1}, true);
        features.add(obj{2}, {{x, 3.5f}, {n, 30}});   // flag starts at the declared initial value, false
        TEST_ASSERT(features.get_latest(flag, obj{1}) == true, "Object 1 flag latest is true");
        TEST_ASSERT(features.get_latest(flag, obj{2}) == false, "Object 2 flag latest is false");
        TEST_ASSERT(features.get_latest(x, obj{2}) == 3.5f, "Object 2 float latest");
        TEST_ASSERT(features.get_latest(n, obj{2}) == 30, "Object 2 int32 latest");
    });

    return 0;
}
