#pragma once

#include "vulcan/runtime_listener.hpp"
#include "vulcan/utils/ordered_multiset.hpp"
#include <deque>

namespace vulcan {

class GlobalRollingPercentileRuntime : public RuntimeListener {
public:
    GlobalRollingPercentileRuntime(int w) : window(w) {}

    void on_update(double val) override { update_impl(val); }
    void on_update(int64_t val) override { update_impl(static_cast<double>(val)); }
    
    Type get_type() const override { return Type::RollingPercentile; }
    static Type static_type() { return Type::RollingPercentile; }
    
    double get_percentile(double p) const {
        return multiset.percentile(p);
    }

private:
    int window;
    OrderedMultiset<double> multiset;
    std::deque<double> history_queue;

    void update_impl(double val) {
        if (window > 0) {
            if (history_queue.size() >= static_cast<size_t>(window)) {
                double removed = history_queue.front();
                history_queue.pop_front();
                multiset.remove(removed);
            }
        }
        history_queue.push_back(val);
        multiset.insert(val);
    }
};

} // namespace vulcan
