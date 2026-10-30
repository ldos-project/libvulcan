#include "vulcan/feature_registry.hpp"
#include <algorithm>

namespace vulcan {

feature_registry::feature_registry() {}

static const feature_desc* find_by_name(const std::vector<feature_desc>& features, const std::string& name) {
    for (const auto& f : features) if (f.name == name) return &f;
    return nullptr;
}

void feature_registry::add_group(std::string name, std::string description) {
    for (const auto& g : groups)
        if (g.name == name) throw std::runtime_error("Group '" + name + "' already declared");
    groups.push_back({std::move(name), std::move(description)});
}

int feature_registry::find_group(const std::string& name) const {
    for (std::size_t i = 0; i < groups.size(); ++i)
        if (groups[i].name == name) return static_cast<int>(i);
    throw std::runtime_error("Group '" + name + "' not declared in registry");
}

static std::string group_name(const std::vector<group_desc>& groups, int group) {
    return group == no_group_index ? "global" : "keyed by '" + groups[group].name + "'";
}

int feature_registry::add_feature(std::string name, std::string description, feature_type type, int group) {
    if (find_by_name(features, name))
        throw std::runtime_error("Feature '" + name + "' already declared");
    feature_desc desc;
    desc.id = next_id++;
    desc.name = std::move(name);
    desc.description = std::move(description);
    desc.type = type;
    desc.group = group;
    desc.scope = group == no_group_index ? feature_desc::scope_type::global : feature_desc::scope_type::object;
    features.push_back(desc);
    return desc.id;
}

// Validate that the name exists and that its type and group match the request
const feature_desc& feature_registry::lookup_checked(const std::string& name, feature_type want_type,
                                                     int want_group) const {
    auto* f = find_by_name(features, name);
    if (!f)
        throw std::runtime_error("Feature '" + name + "' not declared in registry");
    if (f->type != want_type)
        throw std::runtime_error("Feature '" + name + "' is " + feature_type_name(f->type) + ", requested " + feature_type_name(want_type));
    if (f->group != want_group)
        throw std::runtime_error("Feature '" + name + "' is " + group_name(groups, f->group) + ", requested " + group_name(groups, want_group));
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
    const bool per_object = scope == feature_desc::scope_type::object;
    std::vector<std::string> results;
    auto add = [&](auto config) {
        using C = decltype(config);
        if (per_object || !C::per_object_only) results.push_back(C::describe(per_object));
    };
    add(vulcan::listeners::Average{});
    add(vulcan::listeners::MinMax{});
    add(vulcan::listeners::RollingWindow{0});
    add(vulcan::listeners::RollingPercentile{0});
    add(vulcan::listeners::EWMA{0.5});
    add(vulcan::listeners::PopulationPercentile{});
    add(vulcan::listeners::RollingCount{0});
    return results;
}

}
