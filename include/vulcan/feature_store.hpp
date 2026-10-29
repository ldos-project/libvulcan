#pragma once

#include "feature.hpp"
#include "feature_registry.hpp"
#include "store_config.hpp"
#include "listeners.hpp"
#include "vulcan/listeners/average.hpp"
#include "vulcan/listeners/min_max.hpp"
#include "vulcan/listeners/rolling_window.hpp"
#include "vulcan/listeners/rolling_percentile.hpp"
#include "vulcan/listeners/rolling_count.hpp"
#include "vulcan/listeners/population_percentile.hpp"
#include "vulcan/listeners/ewma.hpp"
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
    void update(feature_handle<bool> h, bool val);
    void update(feature_handle<double> h, int64_t obj_id, double val);
    void update(feature_handle<int64_t> h, int64_t obj_id, int64_t val);
    void update(feature_handle<bool> h, int64_t obj_id, bool val);

    // Temporal percentile: over a feature's recent values (RollingPercentile)
    template <typename T>
    double get_temporal_percentile(feature_handle<T> h, double p) const { return get_temporal_percentile(h, global_key, p); }

    template <typename T>
    double get_temporal_percentile(feature_handle<T> h, int64_t obj_id, double p) const {
        auto* listener = get_listener<RollingPercentileRuntime>(h.id);
        return listener->get_percentile(obj_id, p);
    }

    // Population percentile: across every object's latest value (PopulationPercentile)
    template <typename T>
    double get_pop_percentile(feature_handle<T> h, double p) const {
        auto* listener = get_listener<PopulationPercentileRuntime>(h.id);
        return listener->get_percentile(p);
    }

    // MinMax
    template <typename T>
    T get_max(feature_handle<T> h) const { return get_max(h, global_key); }

    template <typename T>
    T get_max(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_listener<MinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_max(obj_id));
    }

    template <typename T>
    T get_min(feature_handle<T> h) const { return get_min(h, global_key); }

    template <typename T>
    T get_min(feature_handle<T> h, int64_t obj_id) const {
        auto* listener = get_listener<MinMaxRuntime>(h.id);
        return static_cast<T>(listener->get_min(obj_id));
    }

    template <typename T>
    double get_avg(feature_handle<T> h) const { return get_avg(h, global_key); }

    template <typename T>
    double get_avg(feature_handle<T> h, int64_t obj_id) const {
        try {
            auto* listener = get_listener<AverageRuntime>(h.id);
            return listener->get_avg(obj_id);
        } catch (...) {
            auto* listener = get_listener<RollingWindowRuntime<T>>(h.id);
            return listener->get_avg(obj_id);
        }
    }

    template <typename T>
    double get_ewma(feature_handle<T> h, double alpha) const { return get_ewma(h, global_key, alpha); }

    template <typename T>
    double get_ewma(feature_handle<T> h, int64_t obj_id, double alpha) const {
        auto* listener = get_listener<EWMARuntime>(h.id);
        return listener->get(obj_id, alpha);
    }

    // Rolling Window
    template <typename T>
    T get_latest(feature_handle<T> h) const { return get_latest(h, global_key); }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, std::size_t k) const { return get_kth_recent(h, global_key, k); }

    template <typename T>
    T get_latest(feature_handle<T> h, int64_t obj_id) const {
        return get_listener<RollingWindowRuntime<T>>(h.id)->get_latest(obj_id);
    }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, int64_t obj_id, std::size_t k) const {
        return get_listener<RollingWindowRuntime<T>>(h.id)->get_kth(obj_id, k);
    }

    // Rolling Count (int64_t or bool features). The value argument is type_identity_t<T> so that T is taken
    // from the handle alone, and a literal like 5 converts to the feature's type instead of conflicting with it.
    template <typename T>
    int get_count(feature_handle<T> h, std::type_identity_t<T> val) const { return get_count(h, global_key, val); }

    template <typename T>
    int get_count(feature_handle<T> h, int64_t obj_id, std::type_identity_t<T> val) const {
        return get_listener<RollingCountRuntime<T>>(h.id)->get_count(obj_id, val);
    }

    template <typename T>
    bool contains(feature_handle<T> h, std::type_identity_t<T> val) const { return contains(h, global_key, val); }

    template <typename T>
    bool contains(feature_handle<T> h, int64_t obj_id, std::type_identity_t<T> val) const {
        return get_listener<RollingCountRuntime<T>>(h.id)->contains(obj_id, val);
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

        // Typed listeners are built with the feature's own value type, and a handle's T is
        // that same type, so a match on Type is a match on the concrete class.
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

// typed listeners: the value type comes from the feature's declaration
static std::unique_ptr<RuntimeListener> make_rolling_window(int window, feature_type t) {
    if (t == feature_type::f64) return std::make_unique<RollingWindowRuntime<double>>(window);
    if (t == feature_type::boolean) return std::make_unique<RollingWindowRuntime<bool>>(window);
    return std::make_unique<RollingWindowRuntime<int64_t>>(window);
}

static std::unique_ptr<RuntimeListener> make_rolling_count(int window, feature_type t) {
    if (t == feature_type::boolean) return std::make_unique<RollingCountRuntime<bool>>(window);
    if (t != feature_type::i64) throw std::invalid_argument("RollingCount listener only supports integer and bool features.");
    return std::make_unique<RollingCountRuntime<int64_t>>(window);
}

// Order statistics are not meaningful on a flag; RollingCount gives the same information.
static void reject_on_bool(feature_type t, const char* listener) {
    if (t == feature_type::boolean)
        throw std::invalid_argument(std::string(listener) + " listener does not support bool features; use RollingCount.");
}

// global listeners
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::Average&, feature_type) { return std::make_unique<AverageRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::MinMax&, feature_type t) { reject_on_bool(t, "MinMax"); return std::make_unique<MinMaxRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingWindow& c, feature_type t) { return make_rolling_window(c.window, t); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingPercentile& c, feature_type t) { reject_on_bool(t, "RollingPercentile"); return std::make_unique<RollingPercentileRuntime>(c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::RollingCount& c, feature_type t) { return make_rolling_count(c.window, t); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::global::EWMA& c, feature_type) { return std::make_unique<EWMARuntime>(c.alphas);}

// object listeners
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::Average&, feature_type) { return std::make_unique<AverageRuntime>();}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::MinMax&, feature_type t) { reject_on_bool(t, "MinMax"); return std::make_unique<MinMaxRuntime>(); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::RollingWindow& c, feature_type t) { return make_rolling_window(c.window, t); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::RollingPercentile& c, feature_type t) { reject_on_bool(t, "RollingPercentile"); return std::make_unique<RollingPercentileRuntime>(c.window); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::RollingCount& c, feature_type t) { return make_rolling_count(c.window, t); }
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::EWMA& c, feature_type) { return std::make_unique<EWMARuntime>(c.alphas);}
static std::unique_ptr<RuntimeListener> make_listener(const vulcan::listeners::object::PopulationPercentile&, feature_type t) { reject_on_bool(t, "PopulationPercentile"); return std::make_unique<PopulationPercentileRuntime>(); }

}
