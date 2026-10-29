#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <sstream>

// Listener configurations: what the evolve block passes to store_config::add_listeners.
// One config type per listener serves global and per-object features alike. describe()
// documents the listener kind for the prompt, with getters for either kind of feature;
// to_string() prints one configured listener as the code that creates it.
namespace vulcan::listeners {

namespace detail {
inline std::string args(bool per_object) { return per_object ? "handle, int64_t obj_id" : "handle"; }
}

struct Average {
    Average() {}
    static constexpr bool per_object_only = false;
    static std::string describe(bool per_object) {
        return "vulcan::listeners::Average()\n"
               "\t- fs.get_avg(" + detail::args(per_object) + ")";
    }
    std::string to_string() const { return "vulcan::listeners::Average()"; }
};

struct MinMax {
    MinMax() {}
    static constexpr bool per_object_only = false;
    static std::string describe(bool per_object) {
        const std::string a = detail::args(per_object);
        return "vulcan::listeners::MinMax()\n"
               "\t- fs.get_max(" + a + ")\n"
               "\t- fs.get_min(" + a + ")";
    }
    std::string to_string() const { return "vulcan::listeners::MinMax()"; }
};

struct RollingWindow {
    int window = 0;
    RollingWindow(int w = 0) : window(w) {}
    static constexpr bool per_object_only = false;
    static std::string describe(bool per_object) {
        const std::string a = detail::args(per_object);
        return "vulcan::listeners::RollingWindow(int window_size)\n"
               "\t  Keeps the last N updates in order; exposes the most-recent value, rolling average, kth-most-recent lookup, and the full window.\n"
               "\t- fs.get_latest(" + a + ")\n"
               "\t- fs.get_kth_recent(" + a + ", int k)\n"
               "\t- fs.get_window_avg(" + a + ")\n"
               "\t- fs.get_all(" + a + ")";
    }
    std::string to_string() const { return "vulcan::listeners::RollingWindow(" + std::to_string(window) + ")"; }
};

struct RollingPercentile {
    int window = 0;
    RollingPercentile(int w) : window(w) {}
    static constexpr bool per_object_only = false;
    static std::string describe(bool per_object) {
        return "vulcan::listeners::RollingPercentile(int window_size)\n"
               "\t  Keeps the last N updates in a sorted structure; computes arbitrary percentiles over that window.\n"
               "\t- fs.get_temporal_percentile(" + detail::args(per_object) + ", double p)";
    }
    std::string to_string() const { return "vulcan::listeners::RollingPercentile(" + std::to_string(window) + ")"; }
};

struct RollingCount {
    int window = 0;
    RollingCount(int w) : window(w) {}
    static constexpr bool per_object_only = false;
    static std::string describe(bool per_object) {
        const std::string a = detail::args(per_object);
        return "vulcan::listeners::RollingCount(int window_size)\n"
               "\t  Counts how many times each distinct integer or bool value appeared in the last N updates; useful for tracking frequency or presence (e.g. how often a specific object was chosen).\n"
               "\t- fs.get_count(" + a + ", int64_t val)\n"
               "\t- fs.contains(" + a + ", int64_t val)";
    }
    std::string to_string() const { return "vulcan::listeners::RollingCount(" + std::to_string(window) + ")"; }
};

struct EWMA {
    std::vector<double> alphas;
    EWMA(std::initializer_list<double> a) : alphas(a) {}
    static constexpr bool per_object_only = false;
    static std::string describe(bool per_object) {
        return "vulcan::listeners::EWMA({alpha, ...})\n"
               "\t  Exponentially weighted moving average; tracks one EWMA per alpha. Higher alpha = faster adaptation to recent changes.\n"
               "\t- fs.get_ewma(" + detail::args(per_object) + ", double alpha)";
    }
    std::string to_string() const {
        std::ostringstream out;
        out << "vulcan::listeners::EWMA({";
        for (size_t i = 0; i < alphas.size(); ++i) out << (i ? ", " : "") << alphas[i];
        out << "})";
        return out.str();
    }
};

// Aggregates across every object's latest value, so it only means something on a per-object feature.
struct PopulationPercentile {
    PopulationPercentile() {}
    static constexpr bool per_object_only = true;
    static std::string describe(bool /*per_object*/) {
        return "vulcan::listeners::PopulationPercentile()\n"
               "\t  Tracks each object's latest value and computes percentiles across the entire object population. Use to compare one object's current value against all others.\n"
               "\t- fs.get_pop_percentile(handle, double p)";
    }
    std::string to_string() const { return "vulcan::listeners::PopulationPercentile()"; }
};

} // namespace vulcan::listeners
