#pragma once

#include "vulcan/runtime_listener.hpp"
#include <string>
#include <type_traits>
#include "vulcan/utils/ordered_multiset.hpp"
#include <deque>
#include <unordered_map>

namespace vulcan {

template <typename T>
class RollingPercentileRuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    RollingPercentileRuntime(int w) : window(w) {}

    void on_update(int64_t obj_id, T val) override { get_stats(obj_id).update(val, window); }
    
    Type get_type() const override { return Type::RollingPercentile; }
    static Type static_type() { return Type::RollingPercentile; }

    // A percentile of flags is not useful; RollingCount gives the same information.
    static constexpr bool accepts = !std::is_same_v<T, bool>;
    static std::string name() { return "RollingPercentile"; }
    
    double get_percentile(int64_t obj_id, double p) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0.0;
        return static_cast<double>(it->second.multiset.percentile(p));
    }

private:
    struct WindowStats {
        OrderedMultiset<T> multiset;
        std::deque<T> history_queue;

        void update(T val, int window) {
            if (window > 0 && history_queue.size() >= static_cast<size_t>(window)) {
                T removed = history_queue.front();
                history_queue.pop_front();
                multiset.remove(removed);
            }
            history_queue.push_back(val);
            multiset.insert(val);
        }
    };

    int window;
    mutable std::unordered_map<int64_t, WindowStats> stats;
    WindowStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
