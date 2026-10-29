#pragma once

#include <cstdint>

namespace vulcan {

// Runtime Listener Interface. Every listener is keyed by object id; a global
// feature is stored as a single object at feature_store::global_key.
class RuntimeListener {
public:
    virtual ~RuntimeListener() = default;

    virtual void on_update(int64_t obj_id, double val) {}
    virtual void on_update(int64_t obj_id, int64_t val) {}

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
    virtual Type get_type() const { return Type::Unknown; }
};

}
