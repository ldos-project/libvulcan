#pragma once

#include "feature.hpp"
#include "listeners.hpp"
#include <map>
#include <vector>

namespace vulcan {

class policy_config {
public:
    virtual ~policy_config() = default;

    void set_information(std::string info) { information_ = std::move(info); }
    const std::string& get_information() const { return information_; }

    template <typename T>
    void add_listeners(feature_handle<T> h, const std::vector<ListenerConfig>& listeners) {
        auto& current_listeners = listeners_[h.id];
        current_listeners.insert(current_listeners.end(), listeners.begin(), listeners.end());
    }

    template <typename T>
    bool has_listeners(feature_handle<T> h) const {
        return listeners_.find(h.id) != listeners_.end() && !listeners_.at(h.id).empty();
    }

    const std::map<int, std::vector<ListenerConfig>>& get_listeners() const {
        return listeners_;
    }

protected:
    std::string information_;
    std::map<int, std::vector<ListenerConfig>> listeners_;
};

} // namespace vulcan
