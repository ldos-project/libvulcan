#pragma once

#include "vulcan/feature.hpp"
#include "vulcan/runtime_listener.hpp"
#include <string>
#include <type_traits>
#include "vulcan/utils/ordered_multiset.hpp"
#include <unordered_map>

namespace vulcan {

template <typename T>
class PopulationPercentileRuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    explicit PopulationPercentileRuntime(const feature_value& initial) : initial(std::get<T>(initial)) {}

    void on_update(int64_t obj_id, T val) override { update_impl(obj_id, val); }

    // A removed member leaves the population
    void erase(int64_t obj_id) override {
        auto it = current_values.find(obj_id);
        if (it == current_values.end()) return;
        multiset.remove(it->second);
        current_values.erase(it);
    }
    
    Type get_type() const override { return Type::PopulationPercentile; }
    static Type static_type() { return Type::PopulationPercentile; }

    // A percentile of flags is not useful; RollingCount gives the same information.
    static constexpr bool accepts = !std::is_same_v<T, bool>;
    static std::string name() { return "PopulationPercentile"; }

    // A group with no members gives the feature's initial value, the value any member starts at
    double get_percentile(double p) const {
        if (multiset.empty()) return static_cast<double>(initial);
        return static_cast<double>(multiset.percentile(p));
    }

private:
    T initial;
    std::unordered_map<int64_t, T> current_values; // obj_id -> current value
    OrderedMultiset<T> multiset;

    void update_impl(int64_t obj_id, T val) {
        auto it = current_values.find(obj_id);
        if (it != current_values.end()) {
            T old_val = it->second;
            multiset.remove(old_val);
        }
        current_values[obj_id] = val;
        multiset.insert(val);
    }
};

} // namespace vulcan
