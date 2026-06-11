#pragma once

#include "vulcan/runtime_listener.hpp"
#include <deque>
#include <vector>

namespace vulcan {

class GlobalRollingWindowRuntime : public RuntimeListener {
public:
    GlobalRollingWindowRuntime(int w) : window(w) {}

    void on_update(double val) override { update_impl(val); }
    void on_update(int64_t val) override { update_impl(val); }
    
    Type get_type() const override { return Type::RollingWindow; }
    static Type static_type() { return Type::RollingWindow; }

    double get_latest_f64() const { return history_f64.empty() ? 0.0 : history_f64.back(); }
    int64_t get_latest_i64() const { return history_i64.empty() ? 0 : history_i64.back(); }
    
    double get_kth_f64(size_t k) const { 
        if (k >= history_f64.size()) return 0.0;
        return history_f64[history_f64.size() - 1 - k];
    }
    int64_t get_kth_i64(size_t k) const {
         if (k >= history_i64.size()) return 0;
        return history_i64[history_i64.size() - 1 - k];
    }

    const std::deque<double>& get_all_f64() const { return history_f64; }
    const std::deque<int64_t>& get_all_i64() const { return history_i64; }

    double get_avg() const {
        if (history_f64.empty() && history_i64.empty()) return 0.0;
        double sum = 0.0;
        size_t count = 0;
        if (!history_f64.empty()) {
            for (double v : history_f64) sum += v;
            count += history_f64.size();
        } else {
            for (int64_t v : history_i64) sum += static_cast<double>(v);
            count += history_i64.size();
        }
        return count > 0 ? (sum / count) : 0.0;
    }

private:
    int window;
    std::deque<double> history_f64;
    std::deque<int64_t> history_i64;

    void update_impl(double val) {
        if (window > 0 && history_f64.size() >= static_cast<size_t>(window)) history_f64.pop_front();
        history_f64.push_back(val);
    }

    void update_impl(int64_t val) {
        if (window > 0 && history_i64.size() >= static_cast<size_t>(window)) history_i64.pop_front();
        history_i64.push_back(val);
    }
};

} // namespace vulcan
