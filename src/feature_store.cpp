#include "vulcan/feature_store.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <variant>
#include "vulcan.h" 

namespace vulcan {

static void init_global_listeners(feature_store::global_feature_data& data, const feature_desc& desc, const std::vector<ListenerConfig>& listeners) {
    data.type = desc.type;
    std::unordered_set<size_t> seen;
    for (const auto& config : listeners) {
        if (!seen.insert(config.index()).second)
            throw std::runtime_error("Duplicate listener type attached to the same global feature");
        std::visit([&](const auto& arg) {
            if constexpr (std::is_same_v<decltype(make_listener(arg)), std::unique_ptr<RuntimeListener>>)
                data.listeners.push_back(make_listener(arg));
        }, config);
    }
}

static void init_object_listeners(feature_store::object_feature_data& data, const feature_desc& desc, const std::vector<ListenerConfig>& listeners) {
    data.type = desc.type;
    std::unordered_set<size_t> seen;
    for (const auto& config : listeners) {
        if (!seen.insert(config.index()).second)
            throw std::runtime_error("Duplicate listener type attached to the same object feature");
        std::visit([&](const auto& arg) {
            if constexpr (std::is_same_v<decltype(make_listener(arg)), std::unique_ptr<ObjectRuntimeListener>>)
                data.listeners.push_back(make_listener(arg));
        }, config);
    }
}

feature_store::feature_store(const feature_registry& reg, const policy_config& config) : registry_(reg) {    
    const auto& user_listeners = config.get_listeners();
    for (const auto& desc : registry_.get_features()) {
        auto it = user_listeners.find(desc.id);
        if (it != user_listeners.end()) {
            if (desc.scope == feature_desc::scope_type::global) {
                if (desc.id >= (int64_t) global_store_vec.size()) global_store_vec.resize(desc.id + 1);
                auto& data = global_store_vec[desc.id];
                init_global_listeners(data, desc, it->second);
            } else if (desc.scope == feature_desc::scope_type::object) {
                // Pre-create the listener containers for object features
                if (desc.id >= (int64_t) object_store_vec.size()) object_store_vec.resize(desc.id + 1);
                auto& data = object_store_vec[desc.id];
                init_object_listeners(data, desc, it->second);
            }
        }
    }
}

feature_store::feature_store(feature_store&& other) noexcept 
    : registry_(other.registry_), 
      global_store_vec(std::move(other.global_store_vec)),
      object_store_vec(std::move(other.object_store_vec)) {
}

feature_store::~feature_store() {
}

void feature_store::update(feature_handle<double> h, double val) {
    if (h.id >= 0 && h.id < (int64_t) global_store_vec.size() && global_store_vec[h.id].type != feature_type::unknown) {
        for (auto& listener : global_store_vec[h.id].listeners) {
            listener->on_update(val);
        }
    }
}

void feature_store::update(feature_handle<int64_t> h, int64_t val) {
    if (h.id >= 0 && h.id < (int64_t) global_store_vec.size() && global_store_vec[h.id].type != feature_type::unknown) {
        for (auto& listener : global_store_vec[h.id].listeners) {
            listener->on_update(val);
        }
    }
}

void feature_store::update(feature_handle<double> h, int64_t obj_id, double val) {
    if (h.id >= 0 && h.id < (int64_t) object_store_vec.size() && object_store_vec[h.id].type != feature_type::unknown) {
        for (auto& listener : object_store_vec[h.id].listeners) {
            listener->on_update(obj_id, val);
        }
    }
}

void feature_store::update(feature_handle<int64_t> h, int64_t obj_id, int64_t val) {
    if (h.id >= 0 && h.id < (int64_t) object_store_vec.size() && object_store_vec[h.id].type != feature_type::unknown) {
        for (auto& listener : object_store_vec[h.id].listeners) {
            listener->on_update(obj_id, val);
        }
    }
}

// Global factory
feature_store instantiate_feature_store(const feature_registry& registry, const policy_config& config) {
    return feature_store(registry, config);
}

}
