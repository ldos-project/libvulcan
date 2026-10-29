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

static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::Average&, feature_type t) { return make_typed<AverageRuntime>(t); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::MinMax&, feature_type t) { return make_typed<MinMaxRuntime>(t); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::RollingWindow& c, feature_type t) { return make_typed<RollingWindowRuntime>(t, c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::RollingPercentile& c, feature_type t) { return make_typed<RollingPercentileRuntime>(t, c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::RollingCount& c, feature_type t) { return make_typed<RollingCountRuntime>(t, c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::EWMA& c, feature_type t) { return make_typed<EWMARuntime>(t, c.alphas); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::PopulationPercentile&, feature_type t) { return make_typed<PopulationPercentileRuntime>(t); }

static void init_listeners(feature_store::feature_data& data, const feature_desc& desc, const std::vector<ListenerConfig>& listeners) {
    data.type = desc.type;
    std::unordered_set<size_t> seen;
    for (const auto& config : listeners) {
        if (!seen.insert(config.index()).second)
            throw std::runtime_error("Duplicate listener type attached to the same feature");
    }
    for (const auto& config : listeners) {
        data.listeners.push_back(std::visit([&](const auto& c) { return make_listener(c, desc.type); }, config));
    }
}

feature_store::feature_store(const feature_registry& reg, const store_config& config) : registry_(reg) {
    const auto& user_listeners = config.get_listeners();
    for (const auto& desc : registry_.get_features()) {
        auto it = user_listeners.find(desc.id);
        if (it != user_listeners.end()) {
            if (desc.id >= (int64_t) store_vec.size()) store_vec.resize(desc.id + 1);
            init_listeners(store_vec[desc.id], desc, it->second);
        }
    }
}

feature_store::feature_store(feature_store&& other) noexcept
    : registry_(other.registry_),
      store_vec(std::move(other.store_vec)) {
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
