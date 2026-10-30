#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "listeners.hpp"

namespace vulcan {

// Forward declaration
class feature_registry;

enum class feature_type {
    f64, 
    f32,
    i64,
    i32,
    boolean,
    unknown
};

// The feature_type for each supported value type. Declaring a feature of any
// other type fails to compile, because the primary template has no definition.
template <typename T> struct feature_type_of;
template <> struct feature_type_of<double>  { static constexpr feature_type value = feature_type::f64; };
template <> struct feature_type_of<float>   { static constexpr feature_type value = feature_type::f32; };
template <> struct feature_type_of<int64_t> { static constexpr feature_type value = feature_type::i64; };
template <> struct feature_type_of<int32_t> { static constexpr feature_type value = feature_type::i32; };
template <> struct feature_type_of<bool>    { static constexpr feature_type value = feature_type::boolean; };

inline const char* feature_type_name(feature_type t) {
    switch (t) {
        case feature_type::f64:     return "f64";
        case feature_type::f32:     return "f32";
        case feature_type::i64:     return "i64";
        case feature_type::i32:     return "i32";
        case feature_type::boolean: return "bool";
        default:                    return "unknown";
    }
}

// Object groups. A group is a C++ type: VULCAN_GROUP(page, "resident cache entries") declares the
// member type `page`, an opaque id, and records the group's name and description. Each member of the
// group is one value of that type. A registry adds the group with registry.declare_group<page>().
// Write VULCAN_GROUP at global scope: it specializes vulcan::group_traits.
template <typename G> struct group_traits;   // defined for each group by VULCAN_GROUP

#define VULCAN_GROUP(Name, Description)                                      \
    enum class Name : int64_t {};                                            \
    template <> struct vulcan::group_traits<Name> {                          \
        static constexpr const char* name = #Name;                           \
        static constexpr const char* description = Description;              \
    }

// The group of global features: a feature keyed by no_group has a single value.
struct no_group {};

// Generic feature handle. G is the object group the feature is keyed by, or no_group for a global feature.
template <typename T, typename G = no_group>
struct feature_handle {
    int id;
    std::string name;
    feature_registry* registry = nullptr;
    
    // Allow implicit comparison
    bool operator==(const feature_handle& other) const { return id == other.id; }
    bool operator!=(const feature_handle& other) const { return id != other.id; }
};

// Common base for registry usage
struct feature_desc {
    int id;
    std::string name;
    std::string description;
    feature_type type;
    int group = -1;   // index into feature_registry::get_groups(), or -1 for a global feature
    enum class scope_type { global, object } scope = scope_type::global;
};

// An object group as recorded by a registry
struct group_desc {
    std::string name;
    std::string description;
};

}