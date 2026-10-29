#pragma once

#include <cstdint>

namespace vulcan {

// Runtime Listener Interface. Every listener is keyed by object id; a global
// feature is stored as a single object at feature_store::global_key.
//
// RuntimeListener is the type-erased base the store keeps in each feature's
// slot. Updates go through TypedRuntimeListener<T>, where T is the feature's
// value type: the store builds every listener on a feature with that feature's
// T, and a feature_handle<T> carries the same T, so the store can call the
// typed update directly.
class RuntimeListener {
public:
    virtual ~RuntimeListener() = default;

    // Identification for queries
    enum class Type {
        Average,
        MinMax,
        RollingWindow,
        RollingPercentile,
        RollingCount,
        EWMA,
        PopulationPercentile,
        Unknown
    };
    virtual Type get_type() const = 0;
};

template <typename T>
class TypedRuntimeListener : public RuntimeListener {
public:
    virtual void on_update(int64_t obj_id, T val) = 0;
};

}
