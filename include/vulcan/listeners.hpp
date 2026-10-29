#pragma once

#include <variant>
#include "vulcan/listeners/object.hpp"

namespace vulcan {

using ListenerConfig = std::variant<
    vulcan::listeners::Average,
    vulcan::listeners::MinMax,
    vulcan::listeners::RollingWindow,
    vulcan::listeners::RollingPercentile,
    vulcan::listeners::RollingCount,
    vulcan::listeners::EWMA,
    vulcan::listeners::PopulationPercentile
>;

} // namespace vulcan
