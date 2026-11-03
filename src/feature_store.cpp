#include "vulcan/feature_store.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <variant>
#include "vulcan.h"

namespace vulcan {

// Builds Listener<T> if the listener accepts T. Each listener states the value types it supports in its
// own `accepts`, so the rules live with the listener and a disallowed combination is never compiled.
template <template <typename> class Listener, typename T, typename... Args>
static std::unique_ptr<RuntimeListener> build(const Args&... args) {
    if constexpr (Listener<T>::accepts)
        return std::make_unique<Listener<T>>(args...);
    else
        throw std::invalid_argument(Listener<T>::name() + " listener does not support " +
                                    feature_type_name(feature_type_of<T>::value) + " features.");
}

// Builds Listener<T> for the feature's value type T. Listener is a listener template rather than a type:
// make_typed<EWMARuntime>(t, alphas) builds EWMARuntime<double> for an f64 feature, EWMARuntime<int64_t> for i64,
// and so on.
template <template <typename> class Listener, typename... Args>
static std::unique_ptr<RuntimeListener> make_typed(feature_type t, const Args&... args) {
    switch (t) {
        case feature_type::f64:     return build<Listener, double>(args...);
        case feature_type::f32:     return build<Listener, float>(args...);
        case feature_type::i64:     return build<Listener, int64_t>(args...);
        case feature_type::i32:     return build<Listener, int32_t>(args...);
        case feature_type::boolean: return build<Listener, bool>(args...);
        default: throw std::invalid_argument("Listener attached to a feature of unknown type.");
    }
}

static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::Average&, const feature_desc& d) { return make_typed<AverageRuntime>(d.type); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::MinMax&, const feature_desc& d) { return make_typed<MinMaxRuntime>(d.type); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::RollingWindow& c, const feature_desc& d) { return make_typed<RollingWindowRuntime>(d.type, c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::RollingPercentile& c, const feature_desc& d) { return make_typed<RollingPercentileRuntime>(d.type, c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::RollingCount& c, const feature_desc& d) { return make_typed<RollingCountRuntime>(d.type, c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::EWMA& c, const feature_desc& d) { return make_typed<EWMARuntime>(d.type, c.alphas); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::PopulationPercentile&, const feature_desc& d) { return make_typed<PopulationPercentileRuntime>(d.type, d.initial); }

static void init_listeners(feature_store::feature_data& data, const feature_desc& desc, const std::vector<ListenerConfig>& listeners) {
    data.type = desc.type;
    std::unordered_set<size_t> seen;
    for (const auto& config : listeners) {
        if (!seen.insert(config.index()).second)
            throw std::runtime_error("Duplicate listener type attached to the same feature");
    }
    for (const auto& config : listeners) {
        data.listeners.push_back(std::visit([&](const auto& c) { return make_listener(c, desc); }, config));
    }
}

feature_store::feature_store(const feature_registry& reg, const store_config& config) : registry_(reg) {
    members_.resize(registry_.get_groups().size());
    group_features_.resize(registry_.get_groups().size());
    const auto& features = registry_.get_features();
    store_vec.resize(features.size());
    const auto& user_listeners = config.get_listeners();
    for (const auto& desc : features) {
        store_vec[desc.id].group = desc.group;
        auto it = user_listeners.find(desc.id);
        if (it == user_listeners.end()) continue;
        init_listeners(store_vec[desc.id], desc, it->second);
        // A global feature starts at its initial value; a keyed feature's members get theirs when added.
        if (desc.group == no_group_index) seed(desc.id, global_key, desc.initial);
        else group_features_[desc.group].push_back(desc.id);
    }
}

feature_store::feature_store(feature_store&& other) noexcept
    : registry_(other.registry_),
      store_vec(std::move(other.store_vec)),
      members_(std::move(other.members_)),
      group_features_(std::move(other.group_features_)),
      rng_(std::move(other.rng_)) {
}

bool feature_store::add_member(int group, int64_t id) {
    auto& m = members_[group];
    if (m.index.count(id)) return false;
    m.index[id] = m.order.size();
    m.order.push_back(id);
    return true;
}

void feature_store::remove_member(int group, int64_t id) {
    auto& m = members_[group];
    auto it = m.index.find(id);
    if (it == m.index.end()) return;
    std::size_t pos = it->second;
    int64_t last = m.order.back();
    m.order[pos] = last;
    m.index[last] = pos;
    m.order.pop_back();
    m.index.erase(id);
    for (int feature : group_features_[group])
        for (auto& l : store_vec[feature].listeners) l->erase(id);
}

// The variant holds the feature's own value type, so this calls push<T> with the feature's T.
void feature_store::seed(int id, int64_t obj_id, const feature_value& value) {
    std::visit([&](auto v) { push(id, obj_id, v); }, value);
}

void feature_store::not_a_member(int id, int64_t obj_id) const {
    const auto& f = registry_.get_features()[id];
    throw std::logic_error("Feature '" + f.name + "' used for " + std::to_string(obj_id) +
                           ", which is not a member of group '" + registry_.get_groups()[f.group].name + "'");
}

// Floyd's algorithm: one random draw per sampled position, so the cost depends on k rather than n.
std::vector<std::size_t> feature_store::sample_positions(std::size_t n, std::size_t k) const {
    std::vector<std::size_t> picked;
    if (k >= n) {
        for (std::size_t i = 0; i < n; ++i) picked.push_back(i);
        return picked;
    }
    picked.reserve(k);
    for (std::size_t j = n - k; j < n; ++j) {
        std::size_t t = std::uniform_int_distribution<std::size_t>(0, j)(rng_);
        bool taken = std::find(picked.begin(), picked.end(), t) != picked.end();
        picked.push_back(taken ? j : t);
    }
    return picked;
}

feature_store::~feature_store() {
}

feature_store instantiate_feature_store(const feature_registry& registry, const store_config& config) {
    return feature_store(registry, config);
}

std::shared_ptr<feature_store> make_shared_feature_store(const feature_registry& registry, const store_config& config) {
    return std::make_shared<feature_store>(registry, config);
}

}
