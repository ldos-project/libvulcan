#pragma once

#include "feature.hpp"
#include "listeners.hpp"
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <stdexcept>

namespace vulcan {

// Forward declaration of feature_registry_listener removed.

class feature_registry {
public:
    feature_registry();

    struct GlobalFeatures {
        feature_registry& r;
        feature_handle<double> declare_f64(std::string name, std::string description);
        feature_handle<int64_t> declare_i64(std::string name, std::string description);
        feature_handle<double> lookup_f64(const std::string& name) const;
        feature_handle<int64_t> lookup_i64(const std::string& name) const;
    } global{*this};

    struct ObjectFeatures {
        feature_registry& r;
        feature_handle<double> declare_f64(std::string name, std::string description);
        feature_handle<int64_t> declare_i64(std::string name, std::string description);
        feature_handle<double> lookup_f64(const std::string& name) const;
        feature_handle<int64_t> lookup_i64(const std::string& name) const;
    } object{*this};

    // Generate prompt context for LLM
    std::string generate_policy_prompt() const;
    static std::vector<std::string> get_available_listeners(feature_desc::scope_type scope);

    const std::vector<feature_desc>& get_features() const { return features; }

private:
    // Low-level declarations (Used by Proxies)
    feature_handle<double> declare_f64(std::string name, std::string description, feature_desc::scope_type scope = feature_desc::scope_type::global);
    feature_handle<int64_t> declare_i64(std::string name, std::string description, feature_desc::scope_type scope = feature_desc::scope_type::global);

    friend struct GlobalFeatures;
    friend struct ObjectFeatures;

    int next_id = 0;
    std::vector<feature_desc> features;
};



}