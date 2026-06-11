#include "test_utils.hpp"
#include <iostream>

class TestFixture {
public:
    vulcan::feature_registry registry;
    vulcan::feature_handle<double> var1;
    vulcan::feature_handle<int64_t> var2;

    TestFixture() {
        var1 = registry.global.declare_f64("var1", "Test Double Variable");
        var2 = registry.global.declare_i64("var2", "Test Int Variable");
    }

    vulcan::feature_store create_store(const vulcan::policy_config& config) {
        return vulcan::instantiate_feature_store(registry, config);
    }
};


int main() {
    vulcan_test::run_test("Safety Check (No Listeners)", []() {
        TestFixture fixture;

        // var2 has NO listeners configured
        
        vulcan::policy_config config;
        auto features = fixture.create_store(config);
        
        auto risky_decision = [&](const vulcan::feature_store& fs) -> double {
            return fs.get_max(fixture.var2); 
        };
        
        try {
            features.update(fixture.var2, 5);
            risky_decision(features);
            TEST_ASSERT(false, "Expected exception was NOT thrown when accessing fully unconfigured feature!");
        } catch (const std::runtime_error& e) {
            // Test passed
        }
    });

    vulcan_test::run_test("Duplicate Listener Enforcement", []() {
        TestFixture fixture;
        vulcan::policy_config config;
        config.add_listeners(fixture.var1, {
            vulcan::listeners::global::RollingCount(3),
            vulcan::listeners::global::RollingCount(5) // duplicate type
        });
        
        bool threw_exception = false;
        try {
            auto features = fixture.create_store(config);
        } catch (const std::runtime_error& e) {
            threw_exception = true;
        }
        TEST_ASSERT(threw_exception == true, "Duplicate listener instance should throw std::runtime_error");
    });

    vulcan_test::run_test("i64 Boundary Values and Overflow", []() {
        TestFixture fixture;
        vulcan::policy_config config;
        
        config.add_listeners(fixture.var2, {
            vulcan::listeners::global::MinMax()
        });
        
        auto features = fixture.create_store(config);
        
        // Test large positive value
        int64_t large_pos = 9000000000000000LL; // safely fits in double precision without precision loss for MinMax
        features.update(fixture.var2, large_pos);
        TEST_ASSERT(features.get_max(fixture.var2) == large_pos, "Large positive int64_t value should be stored correctly");
        
        // Test large negative value
        int64_t large_neg = -9000000000000000LL;
        features.update(fixture.var2, large_neg);
        TEST_ASSERT(features.get_min(fixture.var2) == large_neg, "Large negative int64_t value should be stored correctly");
    });

    vulcan_test::run_test("i64 Overflow Isolation", []() {
        TestFixture fixture;
        vulcan::policy_config config;
        
        config.add_listeners(fixture.var2, {
            vulcan::listeners::global::MinMax()
        });
        auto features = fixture.create_store(config);
        
        // Beyond i64 max: uint64_t converted to int64_t
        uint64_t beyond_max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 5;
        int64_t expected_overflow = static_cast<int64_t>(beyond_max); // Typical 2's complement wrap-around to minimum + 4
        
        features.update(fixture.var2, beyond_max); // Implicitly converted to int64_t here
        
        // Due to int64_t -> double conversion loss of precision in MinMax, we just ensure it wrapped to negative
        TEST_ASSERT(features.get_max(fixture.var2) < 0, "Value beyond i64 should wrap around to negative due to int64_t cast");
    });

    return 0;
}
