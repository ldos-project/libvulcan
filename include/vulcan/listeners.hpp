#pragma once

#include <variant>
#include "vulcan/listeners/global.hpp"
#include "vulcan/listeners/object.hpp"

namespace vulcan {

using ListenerConfig = std::variant<
    vulcan::listeners::global::Average,
    vulcan::listeners::global::MinMax,
    vulcan::listeners::global::RollingWindow,
    vulcan::listeners::global::RollingPercentile,
    vulcan::listeners::global::RollingCount,
    vulcan::listeners::global::EWMA,
    vulcan::listeners::object::Average,
    vulcan::listeners::object::MinMax,
    vulcan::listeners::object::RollingWindow,
    vulcan::listeners::object::RollingPercentile,
    vulcan::listeners::object::RollingCount,
    vulcan::listeners::object::EWMA,
    vulcan::listeners::object::PopulationPercentile
>;

} // namespace vulcan
