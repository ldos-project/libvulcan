#pragma once

#include <string>

namespace vulcan {

class policy_config {
public:
    virtual ~policy_config() = default;

    void set_information(std::string info) { information_ = std::move(info); }
    const std::string& get_information() const { return information_; }

protected:
    std::string information_;
};

} // namespace vulcan
