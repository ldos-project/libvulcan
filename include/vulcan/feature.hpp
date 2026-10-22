#pragma once

#include <string>
#include <vector>
#include "listeners.hpp"

namespace vulcan {

// Forward declaration
class feature_registry;

enum class feature_type {
    f64, 
    i64,
    unknown
};

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