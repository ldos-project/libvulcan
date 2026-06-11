#pragma once

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include <algorithm>

namespace vulcan {

template <typename T>
class OrderedMultiset {
    struct Key {
        T            value;
        std::size_t  uid;
        bool operator<(const Key& o) const {
            if (value != o.value) return value < o.value;  // primary key
            return uid < o.uid;                            // tie-break
        }
    };
    using tree_t = __gnu_pbds::tree<
        Key,
        __gnu_pbds::null_type,
        std::less<Key>,
        __gnu_pbds::rb_tree_tag,
        __gnu_pbds::tree_order_statistics_node_update>;

    tree_t      tree_;
    std::size_t next_uid_ = 0; // grows forever; 64-bit is plenty

public:
    T begin() const {
        if (tree_.empty()) return T{};
        return tree_.begin()->value;
    }

    T end() const {
        if (tree_.empty()) return T{};
        return tree_.rbegin()->value;
    }
    
    bool empty()  const { return tree_.empty(); }
    std::size_t size()  const { return tree_.size(); }

    void insert(const T& x) {
        tree_.insert({x, next_uid_++});
    }

    bool remove(const T& x) {
        auto it = tree_.lower_bound({x, 0});
        if (it != tree_.end() && !(x < it->value)) {   // value equal?
            tree_.erase(it);
            return true;
        }
        return false;
    }

    T percentile(double p) const {
        if (tree_.empty()) return T{};
        p  = std::clamp(p, 0.0, 1.0);
        std::size_t idx = static_cast<std::size_t>(p * (tree_.size() - 1));
        return tree_.find_by_order(idx)->value;
    }
};

}