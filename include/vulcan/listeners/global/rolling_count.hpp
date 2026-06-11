#pragma once

#include "vulcan/runtime_listener.hpp"
#include <deque>
#include <unordered_map>
#include <stdexcept>

namespace vulcan {

class GlobalRollingCountRuntime : public RuntimeListener {
public:
    GlobalRollingCountRuntime(int w) : window(w) {}

    void on_update(double val) override { 
        throw std::invalid_argument("RollingCount listener only supports integer features."); 
    }
    
    void on_update(int64_t val) override { 
        if (window > 0 && history.size() >= static_cast<size_t>(window)) {
            int64_t old_val = history.front();
            history.pop_front();
            auto it = counts.find(old_val);
            assert(it != counts.end());
            if (it->second > 1) it->second--;
            else counts.erase(it);
        }
        history.push_back(val);
        counts[val]++;
    }
    
    Type get_type() const override { return Type::RollingCount; }
    static Type static_type() { return Type::RollingCount; }

    int get_count(int64_t val) const { 
        auto it = counts.find(val);
        if (it != counts.end()) return it->second;
        return 0;
    }

    bool contains(int64_t val) const {
        return get_count(val) > 0;
    }

private:
    int window;
    std::deque<int64_t> history;
    std::unordered_map<int64_t, int> counts;
};

} // namespace vulcan
