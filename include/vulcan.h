#pragma once

#include "vulcan/feature.hpp"
#include "vulcan/feature_registry.hpp"
#include "vulcan/listeners.hpp"
#include "vulcan/feature_store.hpp"
#include "vulcan/rank.hpp"
#include "vulcan/value.hpp"
#include <functional>

namespace vulcan {

feature_store instantiate_feature_store(feature_registry& registry);

}
