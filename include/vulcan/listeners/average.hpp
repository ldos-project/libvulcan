#pragma once

#include "vulcan/runtime_listener.hpp"
#include <string>
#include <unordered_map>

namespace vulcan {

template <typename T>
class AverageRuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    AverageRuntime() {}

    void on_update(int64_t obj_id, T val) override { get_stats(obj_id).update(static_cast<double>(val)); }
    void erase(int64_t obj_id) override { stats.erase(obj_id); }
    
    Type get_type() const override { return Type::Average; }
    static Type static_type() { return Type::Average; }

    static constexpr bool accepts = true;
    static std::string name() { return "Average"; }

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
