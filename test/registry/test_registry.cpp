#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>

// True if looking up `name` as a T feature throws
template <typename T>
static bool lookup_throws(const vulcan::feature_registry& registry, const std::string& name) {
    try {
        registry.lookup<T>(name);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    vulcan_test::run_test("Registry - lookup checks the declared value type", []() {
        vulcan::feature_registry registry;
        registry.declare<bool>("flag", "Test flag", false);
        registry.declare<float>("x", "Test float", 0.0f);
        registry.declare<int32_t>("n", "Test int32", 0);

        TEST_ASSERT(registry.lookup<bool>("flag").name == "flag", "Lookup as bool succeeds");
        TEST_ASSERT(lookup_throws<int64_t>(registry, "flag"), "Lookup as int64_t should fail for a bool feature");
        TEST_ASSERT(lookup_throws<double>(registry, "x"), "Lookup as double should fail for a float feature");
        TEST_ASSERT(lookup_throws<int64_t>(registry, "n"), "Lookup as int64_t should fail for an int32 feature");
    });

    return 0;
}
