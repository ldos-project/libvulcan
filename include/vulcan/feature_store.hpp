#pragma once

#include "feature.hpp"
#include "feature_registry.hpp"
#include "policy_config.hpp"
#include "listeners.hpp"
#include "vulcan/listeners/global/average.hpp"
#include "vulcan/listeners/global/min_max.hpp"
#include "vulcan/listeners/global/rolling_window.hpp"
#include "vulcan/listeners/global/rolling_percentile.hpp"
#include "vulcan/listeners/global/rolling_count.hpp"
#include "vulcan/listeners/global/ewma.hpp"
#include "vulcan/listeners/object/average.hpp"
#include "vulcan/listeners/object/min_max.hpp"
#include "vulcan/listeners/object/rolling_window.hpp"
#include "vulcan/listeners/object/rolling_percentile.hpp"
#include "vulcan/listeners/object/rolling_count.hpp"
#include "vulcan/listeners/object/population_percentile.hpp"
#include "vulcan/listeners/object/ewma.hpp"
#include <vector>
#include <deque>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <type_traits>
#include <string>
#include <memory>
#include <algorithm>

namespace vulcan {

class feature_store {
public:
    explicit feature_store(const feature_registry& registry, const policy_config& config);
    virtual ~feature_store();

    // Prevent copying
    feature_store(const feature_store&) = delete;
    feature_store& operator=(const feature_store&) = delete;

    // Allow moving
    feature_store(feature_store&& other) noexcept;
    feature_store& operator=(feature_store&&) = delete;

    // Update methods
    void update(feature_handle<double> h, double val);
    void update(feature_handle<int64_t> h, int64_t val);
    void update(feature_handle<double> h, int64_t obj_id, double val);
    void update(feature_handle<int64_t> h, int64_t obj_id, int64_t val);

    // Percentile 
    template <typename T>
    double get_percentile(feature_handle<T> h, double p) const {
        if (h.id >= 0 && h.id < global_store_vec.size() && global_store_vec[h.id].type != feature_type::unknown) {
            auto* listener = get_global_listener<GlobalRollingPercentileRuntime>(h.id);
            return listener->get_percentile(p);
        } else {
            auto* listener = get_object_listener<ObjectPopulationPercentileRuntime>(h.id);
            return listener->get_percentile(p);
        }
    }

    template <typename T>
    double get_percentile(feature_handle<T> h, int64_t obj_id, double p) const {
        auto* listener = get_object_listener<ObjectRollingPercentileRuntime>(h.id);
        return listener->get_percentile(obj_id, p);
    }

    // MinMax 
    template <typename T>
    T get_max(feature_handle<T> h) const {
        auto* listener = get_global_listener<GlobalMinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_max());
    }

    template <typename T>
    T get_max(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_object_listener<ObjectMinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_max(obj_id));
    }

    template <typename T>
    T get_min(feature_handle<T> h) const {
        auto* listener = get_global_listener<GlobalMinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_min());
    }

    template <typename T>
    T get_min(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_object_listener<ObjectMinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_min(obj_id));
    }

    template <typename T>
    double get_avg(feature_handle<T> h) const {
        if (h.id >= 0 && h.id < global_store_vec.size() && global_store_vec[h.id].type != feature_type::unknown) {
            try {
                auto* listener = get_global_listener<GlobalAverageRuntime>(h.id);
                return listener->get_avg();
            } catch (...) {
                // Try rolling window if average not found
                auto* listener = get_global_listener<GlobalRollingWindowRuntime>(h.id);
                return listener->get_avg();
            }
        } else {
            throw std::runtime_error("Need obj_id for object feature avg");
        }
    }

    template <typename T>
    double get_avg(feature_handle<T> h, int64_t obj_id) const {
        try {
            auto* listener = get_object_listener<ObjectAverageRuntime>(h.id);
            return listener->get_avg(obj_id);
        } catch (...) {
            // Try rolling window if average not found
            auto* listener = get_object_listener<ObjectRollingWindowRuntime>(h.id);
            return listener->get_avg(obj_id);
        }
    }

    template <typename T>
    double get_ewma(feature_handle<T> h, double alpha) const {
        auto* listener = get_global_listener<GlobalEWMARuntime>(h.id);
        return listener->get(alpha);
    }

    template <typename T>
    double get_ewma(feature_handle<T> h, int64_t obj_id, double alpha) const {
        auto* listener = get_object_listener<ObjectEWMARuntime>(h.id);
        return listener->get(obj_id, alpha);
    }

    // Rolling Window 
    template <typename T>
    T get_latest(feature_handle<T> h) const {
        auto* listener = get_global_listener<GlobalRollingWindowRuntime>(h.id);
        if constexpr (std::is_same_v<T, double>) return listener->get_latest_f64();
        else return listener->get_latest_i64();
    }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, std::size_t k) const {
        auto* listener = get_global_listener<GlobalRollingWindowRuntime>(h.id);
         if constexpr (std::is_same_v<T, double>) return listener->get_kth_f64(k);
         else return listener->get_kth_i64(k);
    }

    template <typename T>
    T get_latest(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_object_listener<ObjectRollingWindowRuntime>(h.id);
        if constexpr (std::is_same_v<T, double>) return listener->get_latest_f64(obj_id);
        else return listener->get_latest_i64(obj_id);
    }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, int64_t obj_id, std::size_t k) const {
        auto* listener = get_object_listener<ObjectRollingWindowRuntime>(h.id);
        if constexpr (std::is_same_v<T, double>) return listener->get_kth_f64(obj_id, k);
        else return listener->get_kth_i64(obj_id, k);
    }

    // Rolling Count
    int get_count(feature_handle<int64_t> h, int64_t val) const {
        auto* listener = get_global_listener<GlobalRollingCountRuntime>(h.id);
        return listener->get_count(val);
    }

    int get_count(feature_handle<int64_t> h, int64_t obj_id, int64_t val) const {
        auto* listener = get_object_listener<ObjectRollingCountRuntime>(h.id);
        return listener->get_count(obj_id, val);
    }

    bool contains(feature_handle<int64_t> h, int64_t val) const {
        auto* listener = get_global_listener<GlobalRollingCountRuntime>(h.id);
        return listener->contains(val);
    }

    bool contains(feature_handle<int64_t> h, int64_t obj_id, int64_t val) const {
        auto* listener = get_object_listener<ObjectRollingCountRuntime>(h.id);
        return listener->contains(obj_id, val);
    }

    struct global_feature_data {
        std::vector<std::unique_ptr<RuntimeListener>> listeners;
        feature_type type = feature_type::unknown;
    };

    struct object_feature_data {
        std::vector<std::unique_ptr<ObjectRuntimeListener>> listeners;
        feature_type type = feature_type::unknown;
    };

private:
    const feature_registry& registry_;
    std::vector<global_feature_data> global_store_vec;
    std::vector<object_feature_data> object_store_vec; 

    template <typename ListenerType>
    const ListenerType* get_global_listener(int id) const {
        if (id < 0 || id >= static_cast<int>(global_store_vec.size()) || global_store_vec[id].type == feature_type::unknown) {
             throw std::runtime_error("Global feature not found in store: id=" + std::to_string(id));
        }
        
        const auto& data = global_store_vec[id];

        RuntimeListener::Type target_type = ListenerType::static_type();

        for (const auto& l : data.listeners) {
            if (l->get_type() == target_type) {
                return static_cast<const ListenerType*>(l.get());
            }
        }
        throw std::runtime_error("Listener not found for feature id=" + std::to_string(id));
    }

    template <typename ListenerType>
    const ListenerType* get_object_listener(int id) const {
        if (id < 0 || id >= static_cast<int>(object_store_vec.size()) || object_store_vec[id].type == feature_type::unknown) {
             throw std::runtime_error("Object feature not found in store: id=" + std::to_string(id));
        }

        const auto& data = object_store_vec[id];

        RuntimeListener::Type target_type = ListenerType::static_type();

        for (const auto& l : data.listeners) {
            if (l->get_type() == target_type) {
                return static_cast<const ListenerType*>(l.get());
            }
        }
        throw std::runtime_error("Listener not found for feature id=" + std::to_string(id));
    }
};

// Global factory
feature_store instantiate_feature_store(const feature_registry& registry, const policy_config& config);

// global listeners
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::Average&) { return std::make_unique<GlobalAverageRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::MinMax&) { return std::make_unique<GlobalMinMaxRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingWindow& c) { return std::make_unique<GlobalRollingWindowRuntime>(c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingPercentile& c) { return std::make_unique<GlobalRollingPercentileRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingCount& c) { return std::make_unique<GlobalRollingCountRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::EWMA& c) { return std::make_unique<GlobalEWMARuntime>(c.alphas);}

// object listeners
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::Average&) { return std::make_unique<ObjectAverageRuntime>();}
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::MinMax&) { return std::make_unique<ObjectMinMaxRuntime>();}
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::RollingWindow& c) { return std::make_unique<ObjectRollingWindowRuntime>(c.window);}
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::RollingPercentile& c) { return std::make_unique<ObjectRollingPercentileRuntime>(c.window);}
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::RollingCount& c) { return std::make_unique<ObjectRollingCountRuntime>(c.window);}
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::EWMA& c) { return std::make_unique<ObjectEWMARuntime>(c.alphas);}
static std::unique_ptr<ObjectRuntimeListener> make_listener(const vulcan::listeners::object::PopulationPercentile&) { return std::make_unique<ObjectPopulationPercentileRuntime>();}

}