#pragma once

#include "vulcan/runtime_listener.hpp"
#include <deque>
#include <unordered_map>
#include <stdexcept>

namespace vulcan {

class ObjectRollingCountRuntime : public ObjectRuntimeListener {
public:
    ObjectRollingCountRuntime(int w) : window(w) {}

    void on_update(int64_t obj_id, double val) override { 
        throw std::invalid_argument("RollingCount listener only supports integer features."); 
    }

    void on_update(int64_t obj_id, int64_t val) override { 
        auto& state = states[obj_id];
        if (window > 0 && state.history.size() >= static_cast<size_t>(window)) {
            int64_t old_val = state.history.front();
            state.history.pop_front();
            auto it = state.counts.find(old_val);
            if (it != state.counts.end()) {
                if (it->second > 1) {
                    it->second--;
                } else {
                    state.counts.erase(it);
                }
            }
        }
        state.history.push_back(val);
        state.counts[val]++;
    }
    
    Type get_type() const override { return Type::RollingCount; }
    static Type static_type() { return Type::RollingCount; }

    int get_count(int64_t obj_id, int64_t val) const { 
        auto state_it = states.find(obj_id);
        if (state_it != states.end()) {
            auto count_it = state_it->second.counts.find(val);
            if (count_it != state_it->second.counts.end()) {
                return count_it->second;
            }
        }
        return 0;
    }

    bool contains(int64_t obj_id, int64_t val) const {
        return get_count(obj_id, val) > 0;
    }

private:
    int window;
    
    struct State {
        std::deque<int64_t> history;
        std::unordered_map<int64_t, int> counts;
    };
    
    std::unordered_map<int64_t, State> states;
};

} // namespace vulcan
