#include "vulcan/feature_registry.hpp"
#include <algorithm>

namespace vulcan {

feature_registry::feature_registry() {}

static const feature_desc* find_by_name(const std::vector<feature_desc>& features, const std::string& name) {
    for (const auto& f : features) if (f.name == name) return &f;
    return nullptr;
}

static const char* scope_name(feature_desc::scope_type s) {
    return s == feature_desc::scope_type::global ? "global" : "object";
}

int feature_registry::add_feature(std::string name, std::string description, feature_type type, feature_desc::scope_type scope) {
    if (find_by_name(features, name))
        throw std::runtime_error("Feature '" + name + "' already declared");
    feature_desc desc;
    desc.id = next_id++;
    desc.name = std::move(name);
    desc.description = std::move(description);
    desc.type = type;
    desc.scope = scope;
    features.push_back(desc);
    return desc.id;
}

// Validate that the name exists and that its type and scope match the request
const feature_desc& feature_registry::lookup_checked(const std::string& name, feature_type want_type,
                                                     feature_desc::scope_type want_scope) const {
    auto* f = find_by_name(features, name);
    if (!f)
        throw std::runtime_error("Feature '" + name + "' not declared in registry");
    if (f->type != want_type)
        throw std::runtime_error("Feature '" + name + "' is " + feature_type_name(f->type) + ", requested " + feature_type_name(want_type));
    if (f->scope != want_scope)
        throw std::runtime_error("Feature '" + name + "' is " + scope_name(f->scope) + "-scoped, requested " + scope_name(want_scope));
    return *f;
}

std::string feature_registry::generate_policy_prompt() const {
    std::string prompt = "Available Features:\n";
    for (const auto& desc : features) {
        prompt += "- " + desc.name + ": " + desc.description + "\n";
    }
    return prompt;
}

std::vector<std::string> feature_registry::get_available_listeners(feature_desc::scope_type scope) {
    std::vector<std::string> results;

    auto check_and_add = [&](auto t) {
        using T = decltype(t);
        bool matches = false;
        if (scope == feature_desc::scope_type::global && T::supports_global) matches = true;
        if (scope == feature_desc::scope_type::object && T::supports_object) matches = true;
        
        if (matches) {
            std::string desc = T::describe();
            // Indent for readability if needed, or keep raw
            results.push_back(desc);
        }
    };
    
    check_and_add(vulcan::listeners::global::Average{});
    check_and_add(vulcan::listeners::global::MinMax{});
    check_and_add(vulcan::listeners::global::RollingWindow{0});
    check_and_add(vulcan::listeners::global::RollingPercentile{0});
    check_and_add(vulcan::listeners::global::EWMA{0.5});
    check_and_add(vulcan::listeners::global::RollingCount{0});
    check_and_add(vulcan::listeners::object::Average{});
    check_and_add(vulcan::listeners::object::MinMax{});
    check_and_add(vulcan::listeners::object::RollingWindow{0});
    check_and_add(vulcan::listeners::object::RollingPercentile{0});
    check_and_add(vulcan::listeners::object::EWMA{0.5});
    check_and_add(vulcan::listeners::object::PopulationPercentile{});
    check_and_add(vulcan::listeners::object::RollingCount{0});
    
    return results;
}

}
