#pragma once

#include "vulcan/runtime_listener.hpp"
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace vulcan {

class GlobalEWMARuntime : public RuntimeListener {
public:
    GlobalEWMARuntime(std::initializer_list<double> alphas) {
        for (double a : alphas)
            ewmas[round_alpha(a)] = 0.0;
    }
    GlobalEWMARuntime(const std::vector<double>& alphas) {
        for (double a : alphas)
            ewmas[round_alpha(a)] = 0.0;
    }

    void on_update(double val) override { update_impl(val); }
    void on_update(int64_t val) override { update_impl(static_cast<double>(val)); }

    Type get_type() const override { return Type::EWMA; }
    static Type static_type() { return Type::EWMA; }

    double get(double alpha) const {
        auto it = ewmas.find(round_alpha(alpha));
        if (it == ewmas.end()) throw std::runtime_error("EWMA alpha not found");
        return it->second;
    }

    double get() const {
        if (ewmas.size() != 1) throw std::runtime_error("get_ewma without alpha requires exactly one alpha configured");
        return ewmas.begin()->second;
    }

private:
    bool initialized = false;
    std::unordered_map<double, double> ewmas;

    static double round_alpha(double a) { return std::round(a * 1000.0) / 1000.0; }

    void update_impl(double val) {
        if (!initialized) {
            for (auto& [alpha, v] : ewmas) v = val;
            initialized = true;
        } else {
            for (auto& [alpha, v] : ewmas)
                v += alpha * (val - v);
        }
    }
};

} // namespace vulcan
