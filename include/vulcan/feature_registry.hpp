#pragma once

#include "feature.hpp"
#include "listeners.hpp"
#include <vector>
#include <map>
#include <string>
#include <utility>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace vulcan {

// Forward declaration of feature_registry_listener removed.

class feature_registry {
public:
    feature_registry();

    // Adds the object group G, declared with VULCAN_GROUP.
    template <typename G>
    void declare_group() {
        add_group(group_traits<G>::name, group_traits<G>::description);
    }

    // Declares a feature with value type T, keyed by object group G; G = no_group declares a global feature.
    template <typename T, typename G = no_group>
    feature_handle<T, G> declare(std::string name, std::string description) {
        int id = add_feature(name, std::move(description), feature_type_of<T>::value, group_index<G>());
        return {id, std::move(name), this};
    }

    // Looks up a declared feature, checking that its value type and group match.
    template <typename T, typename G = no_group>
    feature_handle<T, G> lookup(const std::string& name) const {
        const auto& f = lookup_checked(name, feature_type_of<T>::value, group_index<G>());
        return {f.id, f.name, const_cast<feature_registry*>(this)};
    }

    // Generate prompt context for LLM
    std::string generate_policy_prompt() const;
    static std::vector<std::string> get_available_listeners(feature_desc::scope_type scope);

    const std::vector<feature_desc>& get_features() const { return features; }
    const std::vector<group_desc>& get_groups() const { return groups; }

private:
    template <typename G>
    int group_index() const {
        if constexpr (std::is_same_v<G, no_group>) return -1;
        else return find_group(group_traits<G>::name);
    }

    void add_group(std::string name, std::string description);
    int find_group(const std::string& name) const;   // throws if the group was not declared
    int add_feature(std::string name, std::string description, feature_type type, int group);
    const feature_desc& lookup_checked(const std::string& name, feature_type type, int group) const;

    int next_id = 0;
    std::vector<feature_desc> features;
    std::vector<group_desc> groups;
};




}