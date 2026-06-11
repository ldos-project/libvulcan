#pragma once

#include "vulcan/feature.hpp"
#include "vulcan/feature_registry.hpp"
#include "vulcan/listeners.hpp"
#include "vulcan/store_config.hpp"
#include "vulcan/feature_store.hpp"
#include "vulcan/rank.hpp"
#include "vulcan/value.hpp"
#include <functional>
#include <memory>

namespace vulcan {

std::shared_ptr<feature_store> make_shared_feature_store(const feature_registry& registry, const store_config& config);

}
