// Can't-Be-Late across regions: each tick, run on a cluster type in the current region, or migrate.
//
// Output type: std::variant<cluster, region>. A cluster runs the job where it is; a region migrates there,
// making no progress this tick. The enum class fixes the cluster domain, and a region can only be a member.

#include "vulcan.h"
#include <iostream>
#include <optional>
#include <string>
#include <variant>

// ---- Trusted compute base: groups, the cluster domain, features, and the decision's prototype ----
VULCAN_GROUP(region, "Candidate regions");

enum class cluster { SPOT, ON_DEMAND };

vulcan::feature_registry registry = [] {
    vulcan::feature_registry r;
    r.declare_group<region>();
    return r;
}();

const vulcan::feature_handle<bool, region> has_spot =
    registry.declare<bool, region>("has_spot", "This region had spot capacity this tick", false);
const vulcan::feature_handle<bool, region> is_current =
    registry.declare<bool, region>("is_current", "The job occupies this region", false);
const vulcan::feature_handle<double> elapsed =
    registry.declare<double>("elapsed", "Hours since submission", 0.0);
const vulcan::feature_handle<double> progress =
    registry.declare<double>("progress", "Hours of work completed", 0.0);
const vulcan::feature_handle<double> deadline =
    registry.declare<double>("deadline", "Hours from submission to the deadline", 0.0);
const vulcan::feature_handle<double> duration =
    registry.declare<double>("duration", "Total work in hours", 0.0);

std::variant<cluster, region> decide(const vulcan::feature_store& fs);

// ---- The developer's prompt: the task, what the policy can read, and the functions to write ----
std::string prompt() {
    return "--- Task ---\n"
           "A job must finish its work before its deadline. Each tick, run it on spot or on-demand capacity in its "
           "current region, or migrate to another region.\n\n" +
           registry.describe() +
           "--- Write ---\n"
           "1. void configure(vulcan::store_config& listeners): attach the listeners your function reads.\n"
           "2. std::variant<cluster, region> decide(const vulcan::feature_store& fs)\n"
           "   Return a cluster type (cluster::SPOT or cluster::ON_DEMAND) to run in the current region this tick, "
           "or a region to migrate to, which makes no progress this tick.\n";
}

// ---- Listeners the mechanism reads itself ----
void guaranteed_listeners(vulcan::store_config& listeners) {
    listeners.add_listener(is_current, vulcan::listeners::RollingWindow(1));
    listeners.add_listener(progress, vulcan::listeners::RollingWindow(1));
}

// ---- Synthesized by the LLM ----
// EVOLVE-BLOCK-START
void configure(vulcan::store_config& listeners) {
    listeners.add_listener(has_spot, vulcan::listeners::RollingWindow(1))
        .add_listener(has_spot, vulcan::listeners::EWMA({0.5}));
    listeners.add_listener(elapsed, vulcan::listeners::RollingWindow(1));
    listeners.add_listener(deadline, vulcan::listeners::RollingWindow(1));
    listeners.add_listener(duration, vulcan::listeners::RollingWindow(1));
}

std::variant<cluster, region> decide(const vulcan::feature_store& fs) {
    double time_left = fs.get_latest(deadline) - fs.get_latest(elapsed);
    double work_left = fs.get_latest(duration) - fs.get_latest(progress);
    double slack = time_left - work_left;
    if (slack <= 1.0) return cluster::ON_DEMAND;   // no time left to gamble on spot

    std::optional<region> best;
    double best_availability = 0.0;
    for (region r : fs.all<region>()) {
        if (fs.get_latest(is_current, r)) {
            if (fs.get_latest(has_spot, r)) return cluster::SPOT;
            continue;
        }
        double availability = fs.get_ewma(has_spot, r, 0.5);
        if (!best || availability > best_availability) { best = r; best_availability = availability; }
    }
    if (best && best_availability > 0.5) return *best;   // spot has been reliable there: migrate
    return cluster::ON_DEMAND;
}
// EVOLVE-BLOCK-END

// ---- Mechanism: a simulated job on three regions with rotating spot availability ----
bool spot_available(int64_t r, int t) { return (t + 3 * r) % 7 < 3; }

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--prompt") {
        std::cout << prompt();
        return 0;
    }
    vulcan::store_config listeners;
    guaranteed_listeners(listeners);
    configure(listeners);
    auto store = vulcan::instantiate_feature_store(registry, listeners);

    const double job_duration = 10.0, job_deadline = 16.0;
    store.update(duration, job_duration);
    store.update(deadline, job_deadline);
    for (int64_t r = 0; r < 3; ++r) store.add(region{r}, {{is_current, r == 0}});

    int64_t current = 0;
    double done = 0.0, cost = 0.0;
    for (int t = 0; t < 16 && done < job_duration; ++t) {
        for (int64_t r = 0; r < 3; ++r) store.update(has_spot, region{r}, spot_available(r, t));
        store.update(elapsed, static_cast<double>(t));

        auto action = decide(store);
        std::cout << "[t=" << t << "] region " << current << ": ";
        if (auto* c = std::get_if<cluster>(&action)) {
            bool ran = *c == cluster::ON_DEMAND || spot_available(current, t);
            if (ran) done += 1.0;
            cost += *c == cluster::SPOT ? 1.0 : 3.0;
            std::cout << (*c == cluster::SPOT ? "SPOT" : "ON_DEMAND") << (ran ? "" : " (preempted)");
        } else {
            region to = std::get<region>(action);
            store.update(is_current, region{current}, false);
            current = static_cast<int64_t>(to);
            store.update(is_current, to, true);
            std::cout << "migrate -> region " << current;
        }
        store.update(progress, done);
        std::cout << "   progress " << done << "/" << job_duration << "\n";
    }
    std::cout << (done >= job_duration ? "Finished" : "Missed the deadline") << ", cost " << cost << "\n";
    return 0;
}
