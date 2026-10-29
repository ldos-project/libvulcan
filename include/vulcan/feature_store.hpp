#pragma once

#include "feature.hpp"
#include "feature_registry.hpp"
#include "store_config.hpp"
#include "listeners.hpp"
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
    // A global feature is stored as a single object with this id; real object ids are non-negative.
    static constexpr int64_t global_key = -1;

    explicit feature_store(const feature_registry& registry, const store_config& config);
    virtual ~feature_store();

    // Prevent copying
    feature_store(const feature_store&) = delete;
    feature_store& operator=(const feature_store&) = delete;

    // Allow moving
    feature_store(feature_store&& other) noexcept;
    feature_store& operator=(feature_store&&) = delete;

    template <typename T>
    bool has_listeners(feature_handle<T> h) const {
        return h.id >= 0 && h.id < static_cast<int>(store_vec.size()) &&
               store_vec[h.id].type != feature_type::unknown && !store_vec[h.id].listeners.empty();
    }

    // Update methods
    void update(feature_handle<double> h, double val);
    void update(feature_handle<int64_t> h, int64_t val);
    void update(feature_handle<double> h, int64_t obj_id, double val);
    void update(feature_handle<int64_t> h, int64_t obj_id, int64_t val);

    // Temporal percentile: over a feature's recent values (RollingPercentile)
    template <typename T>
    double get_temporal_percentile(feature_handle<T> h, double p) const { return get_temporal_percentile(h, global_key, p); }

    template <typename T>
    double get_temporal_percentile(feature_handle<T> h, int64_t obj_id, double p) const {
        auto* listener = get_listener<ObjectRollingPercentileRuntime>(h.id);
        return listener->get_percentile(obj_id, p);
    }

    // Population percentile: across every object's latest value (PopulationPercentile)
    template <typename T>
    double get_pop_percentile(feature_handle<T> h, double p) const {
        auto* listener = get_listener<ObjectPopulationPercentileRuntime>(h.id);
        return listener->get_percentile(p);
    }

    // MinMax
    template <typename T>
    T get_max(feature_handle<T> h) const { return get_max(h, global_key); }

    template <typename T>
    T get_max(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_listener<ObjectMinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_max(obj_id));
    }

    template <typename T>
    T get_min(feature_handle<T> h) const { return get_min(h, global_key); }

    template <typename T>
    T get_min(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_listener<ObjectMinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_min(obj_id));
    }

    template <typename T>
    double get_avg(feature_handle<T> h) const { return get_avg(h, global_key); }

    template <typename T>
    double get_avg(feature_handle<T> h, int64_t obj_id) const {
        try {
            auto* listener = get_listener<ObjectAverageRuntime>(h.id);
            return listener->get_avg(obj_id);
        } catch (...) {
            auto* listener = get_listener<ObjectRollingWindowRuntime>(h.id);
            return listener->get_avg(obj_id);
        }
    }

    template <typename T>
    double get_ewma(feature_handle<T> h, double alpha) const { return get_ewma(h, global_key, alpha); }

    template <typename T>
    double get_ewma(feature_handle<T> h, int64_t obj_id, double alpha) const {
        auto* listener = get_listener<ObjectEWMARuntime>(h.id);
        return listener->get(obj_id, alpha);
    }

    // Rolling Window
    template <typename T>
    T get_latest(feature_handle<T> h) const { return get_latest(h, global_key); }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, std::size_t k) const { return get_kth_recent(h, global_key, k); }

    template <typename T>
    T get_latest(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_listener<ObjectRollingWindowRuntime>(h.id);
        if constexpr (std::is_same_v<T, double>) return listener->get_latest_f64(obj_id);
        else return listener->get_latest_i64(obj_id);
    }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, int64_t obj_id, std::size_t k) const {
        auto* listener = get_listener<ObjectRollingWindowRuntime>(h.id);
        if constexpr (std::is_same_v<T, double>) return listener->get_kth_f64(obj_id, k);
        else return listener->get_kth_i64(obj_id, k);
    }

    // Rolling Count
    int get_count(feature_handle<int64_t> h, int64_t val) const { return get_count(h, global_key, val); }

    int get_count(feature_handle<int64_t> h, int64_t obj_id, int64_t val) const {
        auto* listener = get_listener<ObjectRollingCountRuntime>(h.id);
        return listener->get_count(obj_id, val);
    }

    bool contains(feature_handle<int64_t> h, int64_t val) const { return contains(h, global_key, val); }

    bool contains(feature_handle<int64_t> h, int64_t obj_id, int64_t val) const {
        auto* listener = get_listener<ObjectRollingCountRuntime>(h.id);
        return listener->contains(obj_id, val);
    }

    struct feature_data {
        std::vector<std::unique_ptr<RuntimeListener>> listeners;
        feature_type type = feature_type::unknown;
    };

private:
    const feature_registry& registry_;
    std::vector<feature_data> store_vec;

    template <typename ListenerType>
    const ListenerType* get_listener(int id) const {
        if (id < 0 || id >= static_cast<int>(store_vec.size()) || store_vec[id].type == feature_type::unknown) {
             throw std::runtime_error("Feature not found in store: id=" + std::to_string(id));
        }

        const auto& data = store_vec[id];

        RuntimeListener::Type target_type = ListenerType::static_type();

        for (const auto& l : data.listeners) {
            if (l->get_type() == target_type) {
                return static_cast<const ListenerType*>(l.get());
            }
        }
        throw std::runtime_error("Listener not found for feature id=" + std::to_string(id));
    }
};

// Factories
feature_store instantiate_feature_store(const feature_registry& registry, const store_config& config);
std::shared_ptr<feature_store> make_shared_feature_store(const feature_registry& registry, const store_config& config);

// global listeners
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::Average&) { return std::make_unique<ObjectAverageRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::MinMax&) { return std::make_unique<ObjectMinMaxRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingWindow& c) { return std::make_unique<ObjectRollingWindowRuntime>(c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingPercentile& c) { return std::make_unique<ObjectRollingPercentileRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingCount& c) { return std::make_unique<ObjectRollingCountRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::EWMA& c) { return std::make_unique<ObjectEWMARuntime>(c.alphas);}

// object listeners
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::Average&) { return std::make_unique<ObjectAverageRuntime>();}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::MinMax&) { return std::make_unique<ObjectMinMaxRuntime>();}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::RollingWindow& c) { return std::make_unique<ObjectRollingWindowRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::RollingPercentile& c) { return std::make_unique<ObjectRollingPercentileRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::RollingCount& c) { return std::make_unique<ObjectRollingCountRuntime>(c.window);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::EWMA& c) { return std::make_unique<ObjectEWMARuntime>(c.alphas);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::PopulationPercentile&) { return std::make_unique<ObjectPopulationPercentileRuntime>();}

}
