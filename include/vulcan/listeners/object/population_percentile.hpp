#pragma once

#include "vulcan/runtime_listener.hpp"
#include "vulcan/utils/ordered_multiset.hpp"
#include <unordered_map>

namespace vulcan {

class ObjectPopulationPercentileRuntime : public ObjectRuntimeListener {
public:
    ObjectPopulationPercentileRuntime() {}

    void on_update(int64_t obj_id, double val) override { update_impl(obj_id, val); }
    void on_update(int64_t obj_id, int64_t val) override { update_impl(obj_id, static_cast<double>(val)); }
    
    Type get_type() const override { return Type::PopulationPercentile; }
    static Type static_type() { return Type::PopulationPercentile; }

    double get_percentile(double p) const {
        return multiset.percentile(p);
    }

private:
    std::unordered_map<int64_t, double> current_values; // obj_id -> current value
    OrderedMultiset<double> multiset;

    void update_impl(int64_t obj_id, double val) {
        auto it = current_values.find(obj_id);
        if (it != current_values.end()) {
            double old_val = it->second;
            multiset.remove(old_val);
        }
        current_values[obj_id] = val;
        multiset.insert(val);
    }
};

} // namespace vulcan
