#include "test_utils.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

VULCAN_GROUP(page, "Resident cache entries");

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


    vulcan_test::run_test("Registry - describe lists groups, features and listeners", []() {
        vulcan::feature_registry registry;
        registry.declare_group<page>();
        registry.declare<bool>("hit", "Last request was a hit", false);
        registry.declare<float, page>("size", "Page size in KiB", 4.0f);
        const std::string text = registry.describe();

        auto has = [&](const std::string& part) { return text.find(part) != std::string::npos; };
        TEST_ASSERT(has("page: Resident cache entries") && has("fs.sample<G>(k)"), "Groups and their accessors");
        TEST_ASSERT(has("hit (bool, initial value false): Last request was a hit"), "A global feature with its type and initial value");
        TEST_ASSERT(has("Features of each page") && has("size (f32, initial value 4): Page size in KiB"), "A page feature");
        TEST_ASSERT(has("vulcan::listeners::EWMA") && has("fs.get_ewma(handle, member, double alpha)"), "The listener catalogue");
    });

    return 0;
}
