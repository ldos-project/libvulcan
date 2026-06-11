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

    // No direct member update anymore, updates happen via feature_store

    // Convenience getters that delegate to a feature_store-like object
    template <typename Store>
    T get_max(const Store& fs) const { return fs.get_max(*this); }
    template <typename Store>
    T get_max(const Store& fs, int obj_id) const { return fs.get_max(*this, obj_id); }

    template <typename Store>
    T get_min(const Store& fs) const { return fs.get_min(*this); }
    template <typename Store>
    T get_min(const Store& fs, int obj_id) const { return fs.get_min(*this, obj_id); }

    template <typename Store>
    double get_avg(const Store& fs) const { return fs.get_avg(*this); }
    template <typename Store>
    double get_avg(const Store& fs, int obj_id) const { return fs.get_avg(*this, obj_id); }

    template <typename Store>
    double get(const Store& fs) const { return fs.get(*this); }
    template <typename Store>
    double get(const Store& fs, int obj_id) const { return fs.get(*this, obj_id); }

    template <typename Store>
    double get_percentile(const Store& fs, double p) const { return fs.get_percentile(*this, p); }
    template <typename Store>
    double get_percentile(const Store& fs, int obj_id, double p) const { return fs.get_percentile(*this, obj_id, p); }

    template <typename Store>
    T get_latest(const Store& fs) const { return fs.get_latest(*this); }
    template <typename Store>
    T get_latest(const Store& fs, int obj_id) const { return fs.get_latest(*this, obj_id); }

    template <typename Store>
    T get_kth_recent(const Store& fs, std::size_t k) const { return fs.get_kth_recent(*this, k); }
    template <typename Store>
    T get_kth_recent(const Store& fs, int obj_id, std::size_t k) const { return fs.get_kth_recent(*this, obj_id, k); }
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