#pragma once

#include "vulcan/runtime_listener.hpp"
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace vulcan {

class ObjectEWMARuntime : public ObjectRuntimeListener {
public:
    ObjectEWMARuntime(std::initializer_list<double> alphas) {
        for (double a : alphas) this->alphas.push_back(round_alpha(a));
    }
    ObjectEWMARuntime(const std::vector<double>& alphas) {
        for (double a : alphas) this->alphas.push_back(round_alpha(a));
    }

    void on_update(int64_t obj_id, double val) override { get_stats(obj_id).update(val, alphas); }
    void on_update(int64_t obj_id, int64_t val) override { get_stats(obj_id).update(static_cast<double>(val), alphas); }

    Type get_type() const override { return Type::EWMA; }
    static Type static_type() { return Type::EWMA; }

    double get(int64_t obj_id, double alpha) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) return 0.0;
        auto eit = it->second.ewmas.find(round_alpha(alpha));
        if (eit == it->second.ewmas.end()) throw std::runtime_error("EWMA alpha not found");
        return eit->second;
    }

    double get(int64_t obj_id) const {
        if (alphas.size() != 1) throw std::runtime_error("get_ewma without alpha requires exactly one alpha configured");
        return get(obj_id, alphas[0]);
    }

private:
    static double round_alpha(double a) { return std::round(a * 1000.0) / 1000.0; }

    struct EWMAStats {
        bool initialized = false;
        std::unordered_map<double, double> ewmas;
        void update(double val, const std::vector<double>& alphas) {
            if (!initialized) {
                for (double a : alphas) ewmas[a] = val;
                initialized = true;
            } else {
                for (auto& [a, v] : ewmas)
                    v += a * (val - v);
            }
        }
    };

    std::vector<double> alphas;
    mutable std::unordered_map<int64_t, EWMAStats> stats;
    EWMAStats& get_stats(int64_t obj_id) { return stats[obj_id]; }
};

} // namespace vulcan
