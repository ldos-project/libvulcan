#pragma once

#include "feature_store.hpp"
#include "policy_config.hpp"
#include "store_config.hpp"
#include <functional>
#include <iostream>
#include <sstream>
#include <memory>

namespace vulcan {

using value_fn_t = std::function<double(const feature_store&)>;

class value_config : public policy_config {
public:
    void set_value_fn(value_fn_t fn) { value_fn_ = fn; }
    const value_fn_t& get_value_fn() const { return value_fn_; }

private:
    value_fn_t value_fn_;
};

class value_policy {
public:
    value_policy(const feature_registry& registry, const value_config& config,
                 std::shared_ptr<feature_store> store)
        : registry_(registry), store_(std::move(store)), config_(config) {}

    feature_store& get_feature_store() { return *store_; }
    const feature_store& get_feature_store() const { return *store_; }

    double decide() {
        auto& fn = config_.get_value_fn();
        if (!fn) {
            std::cerr << "Vulcan Error: No value function set for value_policy.\n";
            return 0.0;
        }
        return fn(*store_);
    }

    std::string get_prompt() const {
        std::ostringstream ss;
        ss << "=== VULCAN VALUE POLICY ===\n\n";

        const auto& info = config_.get_information();
        if (!info.empty()) {
            ss << "--- Context ---\n";
            ss << info << "\n\n";
        }

        ss << "--- Policy ---\n";
        ss << "Vulcan is a framework for automatically discovering resource-management heuristics using LLM-based code generation. It separates policy (what to do) from mechanism (how to do it), so you only write the decision logic.\n\n";
        ss << "A VALUE policy computes a single scalar output from the current system state (e.g. the next congestion window size, a target queue depth, a timeout duration). You implement your heuristic as a value function that reads from the feature store and returns a double.\n\n";

        const auto& features = registry_.get_features();

        std::vector<const feature_desc*> global_features, object_features;
        for (const auto& f : features) {
            if (f.scope == feature_desc::scope_type::global)
                global_features.push_back(&f);
            else
                object_features.push_back(&f);
        }

        if (!global_features.empty()) {
            ss << "--- Global Features ---\n";
            for (const auto* f : global_features) {
                ss << "  - " << f->name
                   << " (" << (f->type == feature_type::f64 ? "f64" : "i64") << "): "
                   << f->description << "\n";
            }
            ss << "\n";
        }

        if (!object_features.empty()) {
            ss << "--- Per-Object Features ---\n";
            for (const auto* f : object_features) {
                ss << "  - " << f->name
                   << " (" << (f->type == feature_type::f64 ? "f64" : "i64") << "): "
                   << f->description << "\n";
            }
            ss << "\n";
        }

        ss << "--- Listeners ---\n";
        ss << "By default, no listeners are attached to any feature, meaning no data is collected for it. To use a feature in your value function you must attach one or more listeners to it. Listeners configure how data is stored and processed, and expose query functions (e.g. rolling averages, min/max, percentiles) that you call inside your value function. Attach with: store_cfg.add_listeners(handle, {listener1, listener2, ...});\n\n";
        ss << "Listeners that can be attached to global features:\n";
        for (const auto& doc : feature_registry::get_available_listeners(feature_desc::scope_type::global)) {
            ss << "  " << doc << "\n";
        }
        ss << "\n";
        if (!object_features.empty()) {
            ss << "Listeners that can be attached to per-object features:\n";
            for (const auto& doc : feature_registry::get_available_listeners(feature_desc::scope_type::object)) {
                ss << "  " << doc << "\n";
            }
            ss << "\n";
        }

        ss << "--- Expected Output ---\n";
        ss << "1. Listener configuration — attach listeners to each feature you want to use.\n";
        ss << "     store_cfg.add_listeners(handle, {vulcan::listeners::global::RollingWindow(5), vulcan::listeners::global::MinMax()});\n\n";
        ss << "2. Value function — this IS your heuristic. Called each decision step, returns the computed scalar.\n";
        ss << "     auto fn = [&](const vulcan::feature_store& fs) -> double {\n";
        ss << "         // e.g. fs.get_latest(handle), fs.get_max(handle), fs.get_percentile(handle, 0.95)\n";
        ss << "         return <computed_value>;\n";
        ss << "     };\n";
        ss << "     config.set_value_fn(fn);\n";

        return ss.str();
    }

private:
    const feature_registry& registry_;
    std::shared_ptr<feature_store> store_;
    value_config config_;
};

inline value_policy instantiate_value_policy(const feature_registry& registry, const value_config& config,
                                             std::shared_ptr<feature_store> store) {
    return value_policy(registry, config, std::move(store));
}

inline double decision(value_policy& p) {
    return p.decide();
}

}
