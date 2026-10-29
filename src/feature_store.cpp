#include "vulcan/feature_store.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <variant>
#include "vulcan.h"

namespace vulcan {

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
