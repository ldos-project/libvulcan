#pragma once

#include "vulcan/runtime_listener.hpp"
#include <string>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace vulcan {

template <typename T>
class EWMARuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    EWMARuntime(std::initializer_list<double> alphas) {
        for (double a : alphas) this->alphas.push_back(round_alpha(a));
    }
    EWMARuntime(const std::vector<double>& alphas) {
        for (double a : alphas) this->alphas.push_back(round_alpha(a));
    }

    void on_update(int64_t obj_id, T val) override { get_stats(obj_id).update(static_cast<double>(val), alphas); }
    void erase(int64_t obj_id) override { stats.erase(obj_id); }

    Type get_type() const override { return Type::EWMA; }
    static Type static_type() { return Type::EWMA; }

    static constexpr bool accepts = true;
    static std::string name() { return "EWMA"; }

    double get(int64_t obj_id, double alpha) const {
        auto it = stats.find(obj_id);
        if (it == stats.end()) {
            if (std::find(alphas.begin(), alphas.end(), round_alpha(alpha)) == alphas.end())
                throw std::runtime_error("EWMA alpha not found");
            return 0.0;
        }
        auto eit = it->second.ewmas.find(round_alpha(alpha));
        if (eit == it->second.ewmas.end()) throw std::runtime_error("EWMA alpha not found");
        return eit->second;
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
