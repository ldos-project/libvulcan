#include "vulcan/feature_registry.hpp"
#include <sstream>
#include <variant>
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

int feature_registry::add_feature(std::string name, std::string description, feature_type type, int group,
                                  feature_value initial) {
    if (find_by_name(features, name))
        throw std::runtime_error("Feature '" + name + "' already declared");
    feature_desc desc;
    desc.id = next_id++;
    desc.name = std::move(name);
    desc.description = std::move(description);
    desc.type = type;
    desc.initial = initial;
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

static std::string value_string(const feature_value& v) {
    return std::visit([](auto x) -> std::string {
        if constexpr (std::is_same_v<decltype(x), bool>) return x ? "true" : "false";
        else { std::ostringstream out; out << x; return out.str(); }
    }, v);
}

static void list_features(std::ostringstream& ss, const feature_registry& registry, int group) {
    for (const auto& f : registry.get_features()) {
        if (f.group != group) continue;
        ss << "  - " << f.name << " (" << feature_type_name(f.type) << ", initial value " << value_string(f.initial)
           << "): " << f.description << "\n";
    }
}

static std::string features_section(const feature_registry& registry) {
    std::ostringstream ss;
    const auto& groups = registry.get_groups();
    if (!groups.empty()) {
        ss << "--- Object Groups ---\n";
        ss << "Each group is a C++ type whose values are its current members. Get members only from the store: "
              "fs.all<G>() (every member), fs.sample<G>(k) (up to k distinct members at random), fs.count<G>() "
              "(how many). Never construct a member yourself.\n";
        for (const auto& g : groups) ss << "  - " << g.name << ": " << g.description << "\n";
        ss << "\n";
    }
    ss << "--- Features ---\n";
    ss << "Global features (read with fs.get_<statistic>(handle, ...)):\n";
    list_features(ss, registry, no_group_index);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        ss << "Features of each " << groups[i].name << " (read with fs.get_<statistic>(handle, member, ...)):\n";
        list_features(ss, registry, static_cast<int>(i));
    }
    ss << "\n";
    return ss.str();
}

static std::string listeners_section(const feature_registry& registry) {
    std::ostringstream ss;
    ss << "--- Listeners ---\n";
    ss << "A feature records nothing until a listener is attached to it. Each listener keeps one kind of statistic "
          "and provides the getters that read it. Attach with "
          "listeners.add_listener(handle, listener1).add_listener(handle, listener2); attaching a listener to a "
          "feature it does not support, or calling a getter whose listener the feature cannot have, does not "
          "compile.\n\n";
    ss << "Listeners for global features:\n";
    for (const auto& doc : feature_registry::get_available_listeners(feature_desc::scope_type::global)) ss << "  " << doc << "\n";
    if (!registry.get_groups().empty()) {
        ss << "\nListeners for features of a group member:\n";
        for (const auto& doc : feature_registry::get_available_listeners(feature_desc::scope_type::object)) ss << "  " << doc << "\n";
    }
    ss << "\n";
    return ss.str();
}

std::string feature_registry::describe() const {
    return features_section(*this) + listeners_section(*this);
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
