#pragma once

#include "vulcan/runtime_listener.hpp"
#include "vulcan/utils/ordered_multiset.hpp"
#include <deque>
#include <unordered_map>

namespace vulcan {

class ObjectRollingPercentileRuntime : public ObjectRuntimeListener {
public:
    ObjectRollingPercentileRuntime(int w) : window(w) {}

    void on_update(int64_t obj_id, double val) override { get_stats(obj_id).update(val, window); }
    void on_update(int64_t obj_id, int64_t val) override { get_stats(obj_id).update(static_cast<double>(val), window); }
    
    Type get_type() const override { return Type::RollingPercentile; }
    static Type static_type() { return Type::RollingPercentile; }
    
    double get_percentile(int64_t obj_id, double p) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0.0;
        return it->second.multiset.percentile(p);
    }

private:
    struct WindowStats {
        OrderedMultiset<double> multiset;
        std::deque<double> history_queue;

        void update(double val, int window) {
            if (window > 0 && history_queue.size() >= static_cast<size_t>(window)) {
                double removed = history_queue.front();
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
