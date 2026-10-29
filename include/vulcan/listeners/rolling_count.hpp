#pragma once

#include "vulcan/runtime_listener.hpp"
#include <string>
#include <type_traits>
#include <deque>
#include <unordered_map>
#include <stdexcept>

namespace vulcan {

template <typename T>
class RollingCountRuntime : public TypedRuntimeListener<T> {
public:
    using Type = RuntimeListener::Type;

    RollingCountRuntime(int w) : window(w) {}

    void on_update(int64_t obj_id, T val) override { 
        auto& state = states[obj_id];
        if (window > 0 && state.history.size() >= static_cast<size_t>(window)) {
            T old_val = state.history.front();
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

    // Counts are only meaningful on discrete values: int64_t and bool.
    static constexpr bool accepts = std::is_integral_v<T>;
    static std::string name() { return "RollingCount"; }

    int get_count(int64_t obj_id, T val) const { 
        auto state_it = states.find(obj_id);
        if (state_it != states.end()) {
            auto count_it = state_it->second.counts.find(val);
            if (count_it != state_it->second.counts.end()) {
                return count_it->second;
            }
        }
        return 0;
    }

    bool contains(int64_t obj_id, T val) const {
        return get_count(obj_id, val) > 0;
    }

private:
    int window;
    
    struct State {
        std::deque<T> history;
        std::unordered_map<T, int> counts;
    };
    
    std::unordered_map<int64_t, State> states;
};

} // namespace vulcan
