#pragma once

#include "feature_store.hpp"
#include "policy_config.hpp"
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <iostream>
#include <functional>
#include <sstream>

namespace vulcan {

namespace rank {
    enum Mechanism {
        FullSort,
        SampleSort
    };
}

// Comparators
inline bool max(double a, double b) { return a > b; }
inline bool min(double a, double b) { return a < b; }

class rank_config : public policy_config {
public:
    using scoring_fn_t = std::function<double(const feature_store&, int64_t)>;
    using comparator_t = std::function<bool(double, double)>;

    void set_sorting_function(rank::Mechanism mech) { sorting_function_ = mech; }
    void set_scoring_fn(scoring_fn_t fn) { scoring_fn_ = fn; }
    void set_comparator(comparator_t comp) { comparator_ = comp; }

    rank::Mechanism get_sorting_function() const { return sorting_function_; }
    const scoring_fn_t& get_scoring_fn() const { return scoring_fn_; }
    const comparator_t& get_comparator() const { return comparator_; }

private:
    rank::Mechanism sorting_function_ = rank::FullSort;
    scoring_fn_t scoring_fn_;
    comparator_t comparator_ = max; // Default to Maximize score
};

class rank_policy {
public:
    rank_policy(const feature_registry& registry, const rank_config& config)
        : registry_(registry), feature_store_(registry, config), config_(config) {}

    feature_store& get_feature_store() { return feature_store_; }
    const feature_store& get_feature_store() const { return feature_store_; }

    void add_object(int64_t obj_id) {
        if (object_indices_.find(obj_id) == object_indices_.end()) {
            object_indices_[obj_id] = object_vector_.size();
            object_vector_.push_back(obj_id);
        }
    }

    void remove_object(int64_t obj_id) {
        auto it = object_indices_.find(obj_id);
        if (it != object_indices_.end()) {
            size_t idx = it->second;
            int64_t last_obj = object_vector_.back();
            
            object_vector_[idx] = last_obj;
            object_indices_[last_obj] = idx;
            
            object_vector_.pop_back();
            object_indices_.erase(it);
        }
    }

    std::vector<std::pair<int64_t, double>> rank_candidates() {
        if (object_vector_.empty()) return {};

        auto mech = config_.get_sorting_function();
        auto& scorer = config_.get_scoring_fn();
        auto& comp = config_.get_comparator();

        if (!scorer) {
            std::cerr << "Vulcan Error: No scoring function set for rank_policy.\n";
            return {};
        }

        std::vector<std::pair<int64_t, double>> scores;

        // Snapshot candidates
        std::vector<int64_t> candidates;
        if (mech == rank::FullSort) {
            candidates = object_vector_;
        } else if (mech == rank::SampleSort) {
            if (object_vector_.size() > 5) {
                candidates = sample_unique(5);
            } else {
                candidates = object_vector_;
            }
        } else {
            throw std::runtime_error("Invalid mechanism: " + std::to_string(mech));
        }

        // Score Phase
        scores.reserve(candidates.size());
        for (int64_t id : candidates) {
            // Context is now passed explicitly to the scorer function
            double score = scorer(feature_store_, id);
            scores.push_back({id, score});
        }

        // Sort using comparator
        std::sort(scores.begin(), scores.end(), [&](const auto& a, const auto& b) {
            return comp(a.second, b.second); 
        });
        return scores;
    }

    int64_t decide() {
        auto scores = rank_candidates();
        if (scores.empty()) return -1;
        return scores.front().first;
    }

    std::string get_prompt() const {
        std::ostringstream ss;
        ss << "=== VULCAN RANK POLICY ===\n\n";

        // User-provided context — first
        const auto& info = config_.get_information();
        if (!info.empty()) {
            ss << "--- Context ---\n";
            ss << info << "\n\n";
        }

        ss << "--- Features ---\n";
        ss << "You can create this scoring function using the features we have listed below. Some features (i.e. global features) encode information about the system as a whole; other features (i.e. per-object) give you information about every object. You are allowed to use any subset of these features (or all of them) in your scoring function. Be creative!\n\n";
        
        // ss << "You also choose how candidates are enumerated before scoring. FullSort scores all N objects — most accurate. SampleSort scores a random subset — lower latency at the cost of some accuracy.\n\n";

        const auto& features = registry_.get_features();

        // Partition features by scope
        std::vector<const feature_desc*> global_features, object_features;
        for (const auto& f : features) {
            if (f.scope == feature_desc::scope_type::global)
                global_features.push_back(&f);
            else
                object_features.push_back(&f);
        }

        // Global features
        if (!global_features.empty()) {
            ss << "Global Features:\n";
            for (const auto* f : global_features) {
                ss << "  - " << f->name
                   << " (" << (f->type == feature_type::f64 ? "f64" : "i64") << "): "
                   << f->description << "\n";
            }
            ss << "\n";
        }

        // Object features
        if (!object_features.empty()) {
            ss << "Per-Object Features:\n";
            for (const auto* f : object_features) {
                ss << "  - " << f->name
                   << " (" << (f->type == feature_type::f64 ? "f64" : "i64") << "): "
                   << f->description << "\n";
            }
            ss << "\n";
        }

        // Listeners — explained once, both scopes listed together
        ss << "--- Listeners ---\n";
        ss << "To use a feature in your scoring function, you must attach one or more **listeners** to it. Listeners collect and process data for a feature, exposing **query functions** (e.g. rolling averages, min/max, percentiles) that you call from inside your scoring function. By default, no listeners are attached to any feature, meaning no data is collected. Calling a query without attaching the corresponding listener causes a runtime error. Global features take global listeners; per-object features take object listeners. Attach with: config.add_listeners(handle, {listener1, listener2, ...});\n\n";
        ss << "Listeners that can be attached to global features:\n";
        for (const auto& doc : feature_registry::get_available_listeners(feature_desc::scope_type::global)) {
            ss << "  " << doc << "\n";
        }
        ss << "\n";
        ss << "Listeners that can be attached to per-object features:\n";
        for (const auto& doc : feature_registry::get_available_listeners(feature_desc::scope_type::object)) {
            ss << "  " << doc << "\n";
        }
        ss << "\n";

        // Expected output
        ss << "--- Expected Output ---\n";
        ss << "1. Listener configuration — attach listeners to each feature you want to use.\n";
        ss << "     config.add_listeners(handle, {vulcan::listeners::object::RollingWindow(5)});\n";
        ss << "     config.add_listeners(handle, {vulcan::listeners::global::RollingPercentile(100)});\n\n";
        ss << "2. Scoring function — this IS your heuristic. Called once per candidate, returns a scalar. Vulcan ranks all candidates by this score and selects the winner via your comparator.\n";
        ss << "     auto fn = [&](const vulcan::feature_store& fs, int64_t obj_id) -> double {\n";
        ss << "         // per-object: fs.get_latest(handle, obj_id), fs.get_avg(handle, obj_id), ...\n";
        ss << "         // global:     fs.get_latest(handle), fs.get_percentile(handle, 0.95), ...\n";
        ss << "         return <score>;\n";
        ss << "     };\n";
        ss << "     config.set_scoring_fn(fn);\n";
        ss << "     config.set_comparator(vulcan::max); // or vulcan::min\n\n";
        ss << "3. Sampling strategy:\n";
        ss << "     config.set_sorting_function(vulcan::rank::FullSort);   // score all N objects\n";
        ss << "     config.set_sorting_function(vulcan::rank::SampleSort); // score a random subset\n";

        return ss.str();
    }

private:
    const feature_registry& registry_;
    feature_store feature_store_; 
    rank_config config_;
    std::vector<int64_t> object_vector_;
    std::unordered_map<int64_t, size_t> object_indices_;
    std::mt19937 rng_{std::random_device{}()};

    std::vector<int64_t> sample_unique(size_t k) {
        std::uniform_int_distribution<size_t> dist(0, object_vector_.size() - 1);
        size_t picked[5];
        size_t n = 0;
        std::vector<int64_t> result;
        result.reserve(k);
        while (n < k) {
            size_t idx = dist(rng_);
            bool dup = false;
            for (size_t j = 0; j < n; ++j)
                if (picked[j] == idx) { dup = true; break; }
            if (!dup) {
                picked[n++] = idx;
                result.push_back(object_vector_[idx]);
            }
        }
        return result;
    }
};

inline rank_policy instantiate_rank_policy(const feature_registry& registry, const rank_config& config) {
    return rank_policy(registry, config);
}

inline int64_t decision(rank_policy& r) {
    return r.decide();
}

}
