#pragma once

#include "vulcan/runtime_listener.hpp"
#include <unordered_map>

namespace vulcan {

class ObjectAverageRuntime : public ObjectRuntimeListener {
public:
    ObjectAverageRuntime() {}

    void on_update(int64_t obj_id, double val) override { get_stats(obj_id).update(val); }
    void on_update(int64_t obj_id, int64_t val) override { get_stats(obj_id).update(static_cast<double>(val)); }
    
    Type get_type() const override { return Type::Average; }
    static Type static_type() { return Type::Average; }

    double get_avg(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end() || it->second.count == 0) return 0.0;
        return it->second.sum / it->second.count;
    }

private:
    struct AverageStats {
        double sum = 0.0;
        size_t count = 0;
        void update(double val) {
            sum += val;
            count++;
        }
    };

    mutable std::unordered_map<int64_t, AverageStats> stats;
    AverageStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
