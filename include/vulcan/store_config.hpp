#pragma once

#include "feature.hpp"
#include "listeners.hpp"
#include "vulcan/listeners/average.hpp"
#include "vulcan/listeners/min_max.hpp"
#include "vulcan/listeners/rolling_window.hpp"
#include "vulcan/listeners/rolling_percentile.hpp"
#include "vulcan/listeners/rolling_count.hpp"
#include "vulcan/listeners/population_percentile.hpp"
#include "vulcan/listeners/ewma.hpp"
#include <map>
#include <type_traits>
#include <vector>

namespace vulcan {

// The runtime listener each config builds, as a template over the feature's value type
template <typename C> struct runtime_of;
template <> struct runtime_of<listeners::Average> { template <typename T> using type = AverageRuntime<T>; };
template <> struct runtime_of<listeners::MinMax> { template <typename T> using type = MinMaxRuntime<T>; };
template <> struct runtime_of<listeners::RollingWindow> { template <typename T> using type = RollingWindowRuntime<T>; };
template <> struct runtime_of<listeners::RollingPercentile> { template <typename T> using type = RollingPercentileRuntime<T>; };
template <> struct runtime_of<listeners::RollingCount> { template <typename T> using type = RollingCountRuntime<T>; };
template <> struct runtime_of<listeners::EWMA> { template <typename T> using type = EWMARuntime<T>; };
template <> struct runtime_of<listeners::PopulationPercentile> { template <typename T> using type = PopulationPercentileRuntime<T>; };

// Whether listener config C can be attached to a feature with value type T keyed by group G: the listener
// must accept T, and a per-object-only listener needs an object group
template <typename C, typename T, typename G>
concept attachable = runtime_of<C>::template type<T>::accepts && (!C::per_object_only || keyed<G>);

class store_config {
public:
    // Attaches one listener to a feature. Returns the config, so calls chain:
    // store_cfg.add_listener(h, RollingWindow(5)).add_listener(h, EWMA({0.5}));
    template <typename T, typename G, typename C> requires attachable<C, T, G>
    store_config& add_listener(feature_handle<T, G> h, C listener) {
        listeners_[h.id].push_back(listener);
        return *this;
    }

    const std::map<int, std::vector<ListenerConfig>>& get_listeners() const {
        return listeners_;
    }

private:
    std::map<int, std::vector<ListenerConfig>> listeners_;
};

} // namespace vulcan
