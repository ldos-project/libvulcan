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
#include <ranges>

namespace vulcan {

// True for an object group, false for no_group (the group of global features)
template <typename G>
concept keyed = !std::is_same_v<G, no_group>;

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

    template <typename T, typename G>
    bool has_listeners(feature_handle<T, G> h) const {
        return h.id >= 0 && h.id < static_cast<int>(store_vec.size()) &&
               store_vec[h.id].type != feature_type::unknown && !store_vec[h.id].listeners.empty();
    }

    // Membership. The mechanism adds and removes the members of each object group. Removing a member
    // drops its state from every listener on the group's features, so a member that is added again
    // starts fresh.
    template <typename G> requires keyed<G>
    void add(G member) { add_member(registry_.group_index<G>(), key(member)); }   // no-op if already a member

    template <typename G> requires keyed<G>
    void remove(G member) { remove_member(registry_.group_index<G>(), key(member)); }   // no-op if not a member

    template <typename G> requires keyed<G>
    bool contains(G member) const { return members_[registry_.group_index<G>()].index.count(key(member)) != 0; }

    template <typename G> requires keyed<G>
    std::size_t count() const { return members_[registry_.group_index<G>()].order.size(); }

    // Every current member of G, as a view over the store's member list: adding or removing a member
    // invalidates it.
    template <typename G> requires keyed<G>
    auto all() const {
        return members_[registry_.group_index<G>()].order
             | std::views::transform([](int64_t id) { return G{id}; });
    }

    // Each operation has a global form, taking a feature_handle<T> (keyed by no_group) and no key, and a keyed
    // form, taking a feature_handle<T, G> and a member of G. A value argument is type_identity_t<T> so that T
    // comes from the handle alone and a literal like 5 converts to the feature's type.

    // Update methods
    template <typename T>
    void update(feature_handle<T> h, std::type_identity_t<T> val) { push(h.id, global_key, val); }

    template <typename T, typename G> requires keyed<G>
    void update(feature_handle<T, G> h, G member, std::type_identity_t<T> val) { push(h.id, key(member), val); }

    // Temporal percentile: over a feature's recent values (RollingPercentile)
    template <typename T>
    double get_temporal_percentile(feature_handle<T> h, double p) const {
        return listener<RollingPercentileRuntime<T>>(h.id)->get_percentile(global_key, p);
    }

    template <typename T, typename G> requires keyed<G>
    double get_temporal_percentile(feature_handle<T, G> h, G member, double p) const {
        return listener<RollingPercentileRuntime<T>>(h.id)->get_percentile(key(member), p);
    }

    // Population percentile: across the latest value of every member of the feature's group (PopulationPercentile)
    template <typename T, typename G> requires keyed<G>
    double get_pop_percentile(feature_handle<T, G> h, double p) const {
        return listener<PopulationPercentileRuntime<T>>(h.id)->get_percentile(p);
    }

    // MinMax
    template <typename T>
    T get_max(feature_handle<T> h) const { return listener<MinMaxRuntime<T>>(h.id)->get_max(global_key); }

    template <typename T, typename G> requires keyed<G>
    T get_max(feature_handle<T, G> h, G member) const { return listener<MinMaxRuntime<T>>(h.id)->get_max(key(member)); }

    template <typename T>
    T get_min(feature_handle<T> h) const { return listener<MinMaxRuntime<T>>(h.id)->get_min(global_key); }

    template <typename T, typename G> requires keyed<G>
    T get_min(feature_handle<T, G> h, G member) const { return listener<MinMaxRuntime<T>>(h.id)->get_min(key(member)); }

    // Average: the mean of every value the feature has received
    template <typename T>
    double get_avg(feature_handle<T> h) const { return listener<AverageRuntime<T>>(h.id)->get_avg(global_key); }

    template <typename T, typename G> requires keyed<G>
    double get_avg(feature_handle<T, G> h, G member) const { return listener<AverageRuntime<T>>(h.id)->get_avg(key(member)); }

    // Window average: the mean of the values a RollingWindow currently holds
    template <typename T>
    double get_window_avg(feature_handle<T> h) const { return listener<RollingWindowRuntime<T>>(h.id)->get_avg(global_key); }

    template <typename T, typename G> requires keyed<G>
    double get_window_avg(feature_handle<T, G> h, G member) const {
        return listener<RollingWindowRuntime<T>>(h.id)->get_avg(key(member));
    }

    // EWMA
    template <typename T>
    double get_ewma(feature_handle<T> h, double alpha) const { return listener<EWMARuntime<T>>(h.id)->get(global_key, alpha); }

    template <typename T, typename G> requires keyed<G>
    double get_ewma(feature_handle<T, G> h, G member, double alpha) const {
        return listener<EWMARuntime<T>>(h.id)->get(key(member), alpha);
    }

    // Rolling Window
    template <typename T>
    T get_latest(feature_handle<T> h) const { return listener<RollingWindowRuntime<T>>(h.id)->get_latest(global_key); }

    template <typename T, typename G> requires keyed<G>
    T get_latest(feature_handle<T, G> h, G member) const {
        return listener<RollingWindowRuntime<T>>(h.id)->get_latest(key(member));
    }

    template <typename T>
    T get_kth_recent(feature_handle<T> h, std::size_t k) const {
        return listener<RollingWindowRuntime<T>>(h.id)->get_kth(global_key, k);
    }

    template <typename T, typename G> requires keyed<G>
    T get_kth_recent(feature_handle<T, G> h, G member, std::size_t k) const {
        return listener<RollingWindowRuntime<T>>(h.id)->get_kth(key(member), k);
    }

    // Rolling Count (integer and bool features)
    template <typename T>
    int get_count(feature_handle<T> h, std::type_identity_t<T> val) const {
        return listener<RollingCountRuntime<T>>(h.id)->get_count(global_key, val);
    }

    template <typename T, typename G> requires keyed<G>
    int get_count(feature_handle<T, G> h, G member, std::type_identity_t<T> val) const {
        return listener<RollingCountRuntime<T>>(h.id)->get_count(key(member), val);
    }

    template <typename T>
    bool contains(feature_handle<T> h, std::type_identity_t<T> val) const {
        return listener<RollingCountRuntime<T>>(h.id)->contains(global_key, val);
    }

    template <typename T, typename G> requires keyed<G>
    bool contains(feature_handle<T, G> h, G member, std::type_identity_t<T> val) const {
        return listener<RollingCountRuntime<T>>(h.id)->contains(key(member), val);
    }

    struct feature_data {
        std::vector<std::unique_ptr<RuntimeListener>> listeners;
        feature_type type = feature_type::unknown;
        int group = no_group_index;   // the feature's object group
    };

private:
    const feature_registry& registry_;
    std::vector<feature_data> store_vec;

    // The members of one object group. Removal swaps the last member into the hole, so order is not
    // insertion order once anything is removed.
    struct group_members {
        std::vector<int64_t> order;
        std::unordered_map<int64_t, std::size_t> index;   // member id -> position in order
    };
    std::vector<group_members> members_;   // indexed like feature_registry::get_groups()

    void add_member(int group, int64_t id);
    void remove_member(int group, int64_t id);

    // A member's id, which keys its state in every listener
    template <typename G>
    static int64_t key(G member) { return static_cast<int64_t>(member); }

    template <typename T>
    void push(int id, int64_t obj_id, T val) {
        if (id < 0 || id >= static_cast<int>(store_vec.size()) || store_vec[id].type == feature_type::unknown) return;
        // Every listener on this feature was built with the feature's value type, which is this handle's T.
        for (auto& l : store_vec[id].listeners)
            static_cast<TypedRuntimeListener<T>*>(l.get())->on_update(obj_id, val);
    }

    template <typename ListenerType>
    const ListenerType* listener(int id) const {
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

}
