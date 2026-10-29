#pragma once

#include "feature.hpp"
#include "listeners.hpp"
#include <vector>
#include <map>
#include <string>
#include <utility>
#include <functional>
#include <stdexcept>

namespace vulcan {

// Forward declaration of feature_registry_listener removed.

class feature_registry {
public:
    feature_registry();

    struct GlobalFeatures {
        feature_registry& r;
        template <typename T>
        feature_handle<T> declare(std::string name, std::string description) {
            return r.declare<T>(std::move(name), std::move(description), feature_desc::scope_type::global);
        }
        template <typename T>
        feature_handle<T> lookup(const std::string& name) const {
            return r.lookup<T>(name, feature_desc::scope_type::global);
        }
    } global{*this};

    struct ObjectFeatures {
        feature_registry& r;
        template <typename T>
        feature_handle<T> declare(std::string name, std::string description) {
            return r.declare<T>(std::move(name), std::move(description), feature_desc::scope_type::object);
        }
        template <typename T>
        feature_handle<T> lookup(const std::string& name) const {
            return r.lookup<T>(name, feature_desc::scope_type::object);
        }
    } object{*this};

    // Generate prompt context for LLM
    std::string generate_policy_prompt() const;
    static std::vector<std::string> get_available_listeners(feature_desc::scope_type scope);

    const std::vector<feature_desc>& get_features() const { return features; }

private:
    template <typename T>
    feature_handle<T> declare(std::string name, std::string description, feature_desc::scope_type scope) {
        int id = add_feature(name, std::move(description), feature_type_of<T>::value, scope);
        return {id, std::move(name), this};
    }

    template <typename T>
    feature_handle<T> lookup(const std::string& name, feature_desc::scope_type scope) const {
        const auto& f = lookup_checked(name, feature_type_of<T>::value, scope);
        return {f.id, f.name, const_cast<feature_registry*>(this)};
    }

    int add_feature(std::string name, std::string description, feature_type type, feature_desc::scope_type scope);
    const feature_desc& lookup_checked(const std::string& name, feature_type type, feature_desc::scope_type scope) const;

    friend struct GlobalFeatures;
    friend struct ObjectFeatures;

    int next_id = 0;
    std::vector<feature_desc> features;
};



}