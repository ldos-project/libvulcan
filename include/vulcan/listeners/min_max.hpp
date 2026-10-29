#pragma once

#include "vulcan/runtime_listener.hpp"
#include <string>
#include <unordered_map>
#include <limits>
#include <algorithm>

namespace vulcan {

template <typename T>
class MinMaxRuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    MinMaxRuntime() {}

    void on_update(int64_t obj_id, T val) override { get_stats(obj_id).update(val); }
    
    Type get_type() const override { return Type::MinMax; }
    static Type static_type() { return Type::MinMax; }

    static constexpr bool accepts = true;
    static std::string name() { return "MinMax"; }

    T get_min(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return std::numeric_limits<T>::max();
        return it->second.min_val;
    }

    T get_max(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return std::numeric_limits<T>::lowest();
        return it->second.max_val;
    }

private:
    struct MinMaxStats {
        T max_val = std::numeric_limits<T>::lowest();
        T min_val = std::numeric_limits<T>::max();
        void update(T val) {
            if (val > max_val) max_val = val;
            if (val < min_val) min_val = val;
        }
    };

    mutable std::unordered_map<int64_t, MinMaxStats> stats;
    MinMaxStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
