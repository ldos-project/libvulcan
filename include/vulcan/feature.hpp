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
    boolean,
    unknown
};

// The feature_type for each supported value type. Declaring a feature of any
// other type fails to compile, because the primary template has no definition.
template <typename T> struct feature_type_of;
template <> struct feature_type_of<double>  { static constexpr feature_type value = feature_type::f64; };
template <> struct feature_type_of<float>   { static constexpr feature_type value = feature_type::f32; };
template <> struct feature_type_of<int64_t> { static constexpr feature_type value = feature_type::i64; };
template <> struct feature_type_of<bool>    { static constexpr feature_type value = feature_type::boolean; };

inline const char* feature_type_name(feature_type t) {
    switch (t) {
        case feature_type::f64:     return "f64";
        case feature_type::f32:     return "f32";
        case feature_type::i64:     return "i64";
        case feature_type::boolean: return "bool";
        default:                    return "unknown";
    }
}

// Generic feature handle
template <typename T>
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
    enum class scope_type { global, object } scope = scope_type::global;
};

}