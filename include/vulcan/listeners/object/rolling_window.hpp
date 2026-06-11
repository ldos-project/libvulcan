#pragma once

#include "vulcan/runtime_listener.hpp"
#include <vector>
#include <unordered_map>

namespace vulcan {

class ObjectRollingWindowRuntime : public ObjectRuntimeListener {
public:
    ObjectRollingWindowRuntime(int w) : window(w) {}

    void on_update(int64_t obj_id, double val) override { get_stats(obj_id).update(val, window); }
    void on_update(int64_t obj_id, int64_t val) override { get_stats(obj_id).update_i64(val, window); }
    
    Type get_type() const override { return Type::RollingWindow; }
    static Type static_type() { return Type::RollingWindow; }

    double get_latest_f64(int64_t obj_id) const { 
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0.0;
        return it->second.latest(window); 
    }
    
    double get_kth_f64(int64_t obj_id, size_t k) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0.0;
        return it->second.kth(k, window );
    }

    int64_t get_latest_i64(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0;
        return it->second.latest_i64(window);
    }

    int64_t get_kth_i64(int64_t obj_id, size_t k) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0;
        return it->second.kth_i64(k, window);
    }

    std::vector<double> get_all_f64(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end() || it->second.count == 0) return {};
        const auto& ws = it->second;
        if (ws.count < window) return ws.history_f64;
        
        std::vector<double> res(window);
        for (int i = 0; i < window; ++i) {
            res[i] = ws.history_f64[(ws.head + i) % window];
        }
        return res;
    }

    double get_avg(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end() || it->second.count == 0) return 0.0;
        double sum = 0.0;
        for (double v : it->second.history_f64) sum += v;
        return sum / it->second.count;
    }

private:
    struct WindowStats {
        std::vector<double> history_f64;
        std::vector<int64_t> history_i64;
        int head = 0;
        int count = 0;
        int head_i64 = 0;
        int count_i64 = 0;

        void update(double val, int window) {
            if (window <= 0) {
                history_f64.push_back(val);
                count++;
                return;
            }
            if (window == 1) {
                if (history_f64.empty()) history_f64.push_back(val);
                else history_f64[0] = val;
                count = 1;
                return;
            }
            if (count < window) {
                history_f64.push_back(val);
                count++;
            } else {
                history_f64[head] = val;
                head = (head + 1) % window;
            }
        }

        void update_i64(int64_t val, int window) {
            if (window <= 0) {
                history_i64.push_back(val);
                count_i64++;
                return;
            }
            if (window == 1) {
                if (history_i64.empty()) history_i64.push_back(val);
                else history_i64[0] = val;
                count_i64 = 1;
                return;
            }
            if (count_i64 < window) {
                history_i64.push_back(val);
                count_i64++;
            } else {
                history_i64[head_i64] = val;
                head_i64 = (head_i64 + 1) % window;
            }
        }

        double latest(int window) const {
            if (count == 0) return 0.0;
            if (window <= 0 || count < window) return history_f64.back();
            int last = (head == 0) ? window - 1 : head - 1;
            return history_f64[last];
        }

        int64_t latest_i64(int window) const {
            if (count_i64 == 0) return 0;
            if (window <= 0 || count_i64 < window) return history_i64.back();
            int last = (head_i64 == 0) ? window - 1 : head_i64 - 1;
            return history_i64[last];
        }

        double kth(size_t k, int window) const {
            if (k >= static_cast<size_t>(count)) return 0.0;
            if (window <= 0 || count < window) return history_f64[count - 1 - k];
            int last = (head == 0) ? window - 1 : head - 1;
            int idx = (last - k + window) % window;
            return history_f64[idx];
        }

        int64_t kth_i64(size_t k, int window) const {
            if (k >= static_cast<size_t>(count_i64)) return 0;
            if (window <= 0 || count_i64 < window) return history_i64[count_i64 - 1 - k];
            int last = (head_i64 == 0) ? window - 1 : head_i64 - 1;
            int idx = (last - k + window) % window;
            return history_i64[idx];
        }
    };

    int window;
    mutable std::unordered_map<int64_t, WindowStats> stats;
    WindowStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
