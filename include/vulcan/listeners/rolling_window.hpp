#pragma once

#include "vulcan/runtime_listener.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace vulcan {

template <typename T>
class RollingWindowRuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    RollingWindowRuntime(int w) : window(w) {}

    void on_update(int64_t obj_id, T val) override { get_stats(obj_id).update(val, window); }

    Type get_type() const override { return Type::RollingWindow; }
    static Type static_type() { return Type::RollingWindow; }

    static constexpr bool accepts = true;
    static std::string name() { return "RollingWindow"; }

    T get_latest(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return T{};
        return it->second.latest(window);
    }

    T get_kth(int64_t obj_id, size_t k) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return T{};
        return it->second.kth(k, window);
    }

    std::vector<T> get_all(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end() || it->second.count == 0) return {};
        const auto& ws = it->second;
        if (window <= 0 || ws.count < window) return ws.history;

        std::vector<T> res(window);
        for (int i = 0; i < window; ++i) {
            res[i] = ws.history[(ws.head + i) % window];
        }
        return res;
    }

    double get_avg(int64_t obj_id) const {
        auto it = stats.find(obj_id);
        if (it == stats.end() || it->second.count == 0) return 0.0;
        double sum = 0.0;
        for (T v : it->second.history) sum += static_cast<double>(v);
        return sum / it->second.count;
    }

private:
    struct WindowStats {
        std::vector<T> history;
        int head = 0;
        int count = 0;

        void update(T val, int window) {
            if (window <= 0) {
                history.push_back(val);
                count++;
                return;
            }
            if (window == 1) {
                if (history.empty()) history.push_back(val);
                else history[0] = val;
                count = 1;
                return;
            }
            if (count < window) {
                history.push_back(val);
                count++;
            } else {
                history[head] = val;
                head = (head + 1) % window;
            }
        }

        T latest(int window) const {
            if (count == 0) return T{};
            if (window <= 0 || count < window) return history.back();
            int last = (head == 0) ? window - 1 : head - 1;
            return history[last];
        }

        T kth(size_t k, int window) const {
            if (k >= static_cast<size_t>(count)) return T{};
            if (window <= 0 || count < window) return history[count - 1 - k];
            int last = (head == 0) ? window - 1 : head - 1;
            int idx = (last - k + window) % window;
            return history[idx];
        }
    };

    int window;
    mutable std::unordered_map<int64_t, WindowStats> stats;
    WindowStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
