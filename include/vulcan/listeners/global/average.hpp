#pragma once

#include "vulcan/runtime_listener.hpp"

namespace vulcan {

class GlobalAverageRuntime : public RuntimeListener {
public:
    GlobalAverageRuntime() {}

    void on_update(double val) override { update_impl(val); }
    void on_update(int64_t val) override { update_impl(static_cast<double>(val)); }
    
    // We update the Type enum in runtime_listener.hpp later
    Type get_type() const override { return Type::Average; }
    static Type static_type() { return Type::Average; }

    double get_avg() const { return count > 0 ? (global_sum / count) : 0.0; }

private:
    double global_sum = 0.0;
    size_t count = 0;

    void update_impl(double val) {
        global_sum += val;
        count++;
    }
};

} // namespace vulcan
