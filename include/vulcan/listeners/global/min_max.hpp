#pragma once

#include "vulcan/runtime_listener.hpp"
#include <limits>
#include <algorithm>

namespace vulcan {

class GlobalMinMaxRuntime : public RuntimeListener {
public:
    GlobalMinMaxRuntime() {}

    void on_update(double val) override { update_impl(val); }
    void on_update(int64_t val) override { update_impl(static_cast<double>(val)); }
    
    Type get_type() const override { return Type::MinMax; }
    static Type static_type() { return Type::MinMax; }

    double get_min() const { return min_val; }
    double get_max() const { return max_val; }

private:
    double max_val = std::numeric_limits<double>::lowest();
    double min_val = std::numeric_limits<double>::max();

    void update_impl(double val) {
        if (val > max_val) max_val = val;
        if (val < min_val) min_val = val;
    }
};

} // namespace vulcan
