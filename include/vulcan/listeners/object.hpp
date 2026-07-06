#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <sstream>

namespace vulcan::listeners::object {

struct Average {
    Average() {}
    static std::string name() { return "object::Average"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::Average()\n"
               "\t- fs.get_avg(handle, int64_t obj_id)";
    }
    std::string get_documentation() const { return describe(); }
};

struct MinMax {
    MinMax() {}
    static std::string name() { return "object::MinMax"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::MinMax()\n"
               "\t- fs.get_max(handle, int64_t obj_id)\n"
               "\t- fs.get_min(handle, int64_t obj_id)";
    }
    std::string get_documentation() const { return describe(); }
};

struct RollingWindow {
    int window = 0;
    RollingWindow(int w = 0) : window(w) {}
    static std::string name() { return "object::RollingWindow"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::RollingWindow(int window_size)\n"
               "\t  Keeps the last N updates per object in order; exposes the most-recent value, rolling average, kth-most-recent lookup, and the full window.\n"
               "\t- fs.get_latest(handle, int64_t obj_id)\n"
               "\t- fs.get_kth_recent(handle, int64_t obj_id, int k)\n"
               "\t- fs.get_avg(handle, int64_t obj_id)\n"
               "\t- fs.get_all(handle, int64_t obj_id)";
    }
    std::string get_documentation() const {
        return describe() + "\n(Current Instance Window: " + (window > 0 ? std::to_string(window) : "infinite") + ")";
    }
};

struct RollingPercentile {
    int window = 0;
    RollingPercentile(int w) : window(w) {}
    static std::string name() { return "object::RollingPercentile"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::RollingPercentile(int window_size)\n"
               "\t  Keeps the last N updates per object in a sorted structure; computes arbitrary percentiles over that per-object window.\n"
               "\t- fs.get_percentile(handle, int64_t obj_id, double p)";
    }
    std::string get_documentation() const {
        return describe() + "\n(Current Instance Window: " + std::to_string(window) + ")";
    }
};

struct RollingCount {
    int window = 0;
    RollingCount(int w) : window(w) {}
    static std::string name() { return "object::RollingCount"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::RollingCount(int window_size)\n"
               "\t  Counts how many times each distinct integer value appeared in the last N updates for a given object; useful for per-object frequency or presence checks.\n"
               "\t- fs.get_count(handle, int64_t obj_id, int64_t val)\n"
               "\t- fs.contains(handle, int64_t obj_id, int64_t val)";
    }
    std::string get_documentation() const {
        return describe() + "\n(Current Instance Window: " + std::to_string(window) + ")";
    }
};

struct EWMA {
    std::vector<double> alphas;
    EWMA(double a) : alphas{a} {}
    EWMA(std::initializer_list<double> a) : alphas(a) {}
    static std::string name() { return "object::EWMA"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::EWMA({alpha, ...})\n"
               "\t  Exponentially weighted moving average per object; tracks one EWMA per alpha. Higher alpha = faster adaptation to recent changes.\n"
               "\t- fs.get_ewma(handle, int64_t obj_id, double alpha)";
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

struct PopulationPercentile {
    PopulationPercentile() {} 
    static std::string name() { return "object::PopulationPercentile"; }
    static constexpr bool supports_global = false;
    static constexpr bool supports_object = true;
    static std::string describe() {
        return "vulcan::listeners::object::PopulationPercentile()\n"
               "\t  Tracks each object's latest value and computes percentiles across the entire object population. Use to compare one object's current value against all others.\n"
               "\t- fs.get_percentile(handle, double p)";
    }
    std::string get_documentation() const { return describe(); }
};

} // namespace vulcan::listeners::object
