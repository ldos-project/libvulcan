#pragma once

#include "feature.hpp"
#include "listeners.hpp"
#include <map>
#include <vector>

namespace vulcan {

class store_config {
public:
    template <typename T>
    void add_listeners(feature_handle<T> h, const std::vector<ListenerConfig>& listeners) {
        auto& current = listeners_[h.id];
        current.insert(current.end(), listeners.begin(), listeners.end());
    }

    const std::map<int, std::vector<ListenerConfig>>& get_listeners() const {
        return listeners_;
    }

private:
    std::map<int, std::vector<ListenerConfig>> listeners_;
};

} // namespace vulcan
