#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <sstream>

namespace vulcan::listeners::global {

struct Average {
    Average() {}
    static std::string name() { return "global::Average"; }
    static constexpr bool supports_global = true;
    static constexpr bool supports_object = false;
    static std::string describe() {
        return "vulcan::listeners::global::Average()\n"
               "\t- fs.get_avg(handle)";
    }
    std::string get_documentation() const { return describe(); }
};

struct MinMax {
    MinMax() {}
    static std::string name() { return "global::MinMax"; }
    static constexpr bool supports_global = true;
    static constexpr bool supports_object = false;
    static std::string describe() {
        return "vulcan::listeners::global::MinMax()\n"
               "\t- fs.get_max(handle)\n"
               "\t- fs.get_min(handle)";
    }
    std::string get_documentation() const { return describe(); }
};

struct RollingWindow {
    int window = 0;
    RollingWindow(int w = 0) : window(w) {}
    static std::string name() { return "global::RollingWindow"; }
    static constexpr bool supports_global = true;
    static constexpr bool supports_object = false;
    static std::string describe() {
        return "vulcan::listeners::global::RollingWindow(int window_size)\n"
               "\t  Keeps the last N updates in order; exposes the most-recent value, rolling average, kth-most-recent lookup, and the full window.\n"
               "\t- fs.get_latest(handle)\n"
               "\t- fs.get_kth_recent(handle, int k)\n"
               "\t- fs.get_avg(handle)\n"
               "\t- fs.get_all(handle)";
    }
    std::string get_documentation() const {
        return describe() + "\n(Current Instance Window: " + (window > 0 ? std::to_string(window) : "infinite") + ")";
    }
};

struct RollingPercentile {
    int window = 0;
    RollingPercentile(int w) : window(w) {}
    static std::string name() { return "global::RollingPercentile"; }
    static constexpr bool supports_global = true;
    static constexpr bool supports_object = false;
    static std::string describe() {
        return "vulcan::listeners::global::RollingPercentile(int window_size)\n"
               "\t  Keeps the last N updates in a sorted structure; computes arbitrary percentiles over that window.\n"
               "\t- fs.get_percentile(handle, double p)";
    }
    std::string get_documentation() const {
        return describe() + "\n(Current Instance Window: " + std::to_string(window) + ")";
    }
};

struct RollingCount {
    int window = 0;
    RollingCount(int w) : window(w) {}
    static std::string name() { return "global::RollingCount"; }
    static constexpr bool supports_global = true;
    static constexpr bool supports_object = false;
    static std::string describe() {
        return "vulcan::listeners::global::RollingCount(int window_size)\n"
               "\t  Counts how many times each distinct integer value appeared in the last N updates; useful for tracking frequency or presence (e.g. how often a specific object was chosen).\n"
               "\t- fs.get_count(handle, int64_t val)\n"
               "\t- fs.contains(handle, int64_t val)";
    }
    std::string get_documentation() const {
        return describe() + "\n(Current Instance Window: " + std::to_string(window) + ")";
    }
};

struct EWMA {
    std::vector<double> alphas;
    EWMA(std::initializer_list<double> a) : alphas(a) {}
    static std::string name() { return "global::EWMA"; }
    static constexpr bool supports_global = true;
    static constexpr bool supports_object = false;
    static std::string describe() {
        return "vulcan::listeners::global::EWMA({alpha, ...})\n"
               "\t  Exponentially weighted moving average; tracks one EWMA per alpha. Higher alpha = faster adaptation to recent changes.\n"
               "\t- fs.get_ewma(handle, double alpha)";
    }
    std::string get_documentation() const {
        std::ostringstream oss;
        oss << describe() << "\n(Current Instance Alphas: {";
        for (size_t i = 0; i < alphas.size(); ++i) {
            if (i) oss << ", ";
            oss << alphas[i];
        }
        oss << "})";
        return oss.str();
    }
};

} // namespace vulcan::listeners::global
