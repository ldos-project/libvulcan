#include "vulcan/feature_store.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vulcan {

// Internal modular components
template <typename T>
class HistoryBuffer {
    std::deque<T> buffer;
    size_t capacity;
public:
    HistoryBuffer(size_t cap) : capacity(cap) {}
    
    void push(T val) {
        buffer.push_front(val);
        size_t limit = std::max(capacity, (size_t)1);
        while (buffer.size() > limit) {
             buffer.pop_back();
        }
    }

    T get_latest() const {
        if (buffer.empty()) return T{};
        return buffer.front();
    }

    T get_kth(size_t k) const {
        if (k < buffer.size()) return buffer[k];
        return T{};
    }

    bool empty() const { return buffer.empty(); }
};

template <typename T>
class MinMaxTracker {
    T min_val = std::numeric_limits<T>::max();
    T max_val = std::numeric_limits<T>::lowest(); // For float, lowest is -max. min is smallest positive.
    bool initialized = false;
    bool track_min = false;
    bool track_max = false;

public:
    MinMaxTracker(bool t_min, bool t_max) : track_min(t_min), track_max(t_max) {
        // Fix for int64 limits
        if constexpr (std::is_integral<T>::value) {
             min_val = std::numeric_limits<T>::max();
             max_val = std::numeric_limits<T>::min();
        }
    }

    void update(T val) {
        if (!initialized) {
            if (track_min) min_val = val;
            if (track_max) max_val = val;
            initialized = true;
        } else {
            if (track_min) min_val = std::min(min_val, val);
            if (track_max) max_val = std::max(max_val, val);
        }
    }

    T get_min() const { return min_val; }
    T get_max() const { return max_val; }
    
    // Config getters
    bool tracking_min() const { return track_min; }
    bool tracking_max() const { return track_max; }
    bool has_data() const { return initialized; }
};
}
