#pragma once

#include "vulcan/runtime_listener.hpp"
#include <unordered_map>
#include <limits>
#include <algorithm>

namespace vulcan {

class ObjectMinMaxRuntime : public ObjectRuntimeListener {
public:
    ObjectMinMaxRuntime() {}

    void on_update(int64_t obj_id, double val) override { get_stats(obj_id).update(val); }
    void on_update(int64_t obj_id, int64_t val) override { get_stats(obj_id).update(static_cast<double>(val)); }
    
    Type get_type() const override { return Type::MinMax; }
    static Type static_type() { return Type::MinMax; }

    double get_min(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return std::numeric_limits<double>::max();
        return it->second.min_val;
    }

    double get_max(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return std::numeric_limits<double>::lowest();
        return it->second.max_val;
    }

private:
    struct MinMaxStats {
        double max_val = std::numeric_limits<double>::lowest();
        double min_val = std::numeric_limits<double>::max();
        void update(double val) {
            if (val > max_val) max_val = val;
            if (val < min_val) min_val = val;
        }
    };

    mutable std::unordered_map<int64_t, MinMaxStats> stats;
    MinMaxStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
