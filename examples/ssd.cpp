#include "vulcan.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

VULCAN_GROUP(ssd, "SSDs that can serve an I/O request");

void* get_request(){
    return nullptr;
}

double get_system_load(const int t){
    return t * 10.0;
}

double get_cpu_usage(const int t){
    return t * 10.0;
}

double get_ssd_latency(const int ssd_id, const int t){
    if(ssd_id == 1) return 10.0 + (t * 30.0); // gets worse over time
    else return 150.0 - (t * 30.0); // gets better over time
}

int get_ssd_temp(const int ssd_id, const int t){
    if(ssd_id == 1) return 30 + (t * 10); // gets worse over time
    else return 90 - (t * 10); // gets better over time
}

void dispatch_io(int t, ssd best, void* request){
    std::cout << "[t= " << t << "]  Dispatching I/O request to drive: " << static_cast<int64_t>(best) << "\n";
}

// setup of global + per object features
vulcan::feature_registry registry = [] {
    vulcan::feature_registry r;
    r.declare_group<ssd>();
    return r;
}();
const auto system_load = registry.declare<double>("system_load", "Current system load average", 0.0);
const auto cpu_usage   = registry.declare<double>("cpu_usage", "CPU usage %", 0.0);
const auto latency = registry.declare<double, ssd>("latency", "Avg latency (ms)", 0.0);
const auto temp    = registry.declare<int64_t, ssd>("temp", "Drive temperature (C)", 0);
const auto chosen_last =
    registry.declare<bool, ssd>("chosen_last", "Whether this SSD served the previous request", false);

// The functions the LLM writes: the listeners it reads, and a score for one SSD. Each request goes to the
// SSD with the lowest score.
void configure(vulcan::store_config& store_cfg);
double score(const vulcan::feature_store& fs, ssd obj_id);

std::string prompt() {
    return "--- Task ---\n"
           "You are building a policy to dispatch I/O requests to SSDs. "
           "Whenever you receive an I/O request, this policy will be invoked to decide "
           "which SSD would be the best one to route the request to. You will receive "
           "features such as latency and temps for each SSD as well as some system-wide"
           "features like load and cpu_usage, and whether each SSD served the previous request\n\n" +
           registry.describe() +
           "--- Write ---\n"
           "1. void configure(vulcan::store_config& store_cfg): attach the listeners your function reads.\n"
           "2. double score(const vulcan::feature_store& fs, ssd obj_id)\n"
           "   Return a score for one SSD. Each request goes to the SSD with the lowest score.\n";
}

// EVOLVE-BLOCK-START
void configure(vulcan::store_config& store_cfg) {
    store_cfg.add_listener(system_load, vulcan::listeners::RollingWindow(1))
        .add_listener(system_load, vulcan::listeners::RollingPercentile(100));
    store_cfg.add_listener(latency, vulcan::listeners::RollingWindow(5));
    store_cfg.add_listener(temp, vulcan::listeners::RollingWindow(5));
    store_cfg.add_listener(chosen_last, vulcan::listeners::RollingWindow(1));
}

double score(const vulcan::feature_store& fs, ssd obj_id) {
    double l = fs.get_latest(latency, obj_id);
    double t = fs.get_latest(temp, obj_id);
    auto score = l + t;
    if(!fs.get_latest(chosen_last, obj_id)) return score += 20; // SSD switching overhead
    return score;
}
// EVOLVE-BLOCK-END

int main() {
    vulcan::store_config store_cfg;
    configure(store_cfg);

    auto store = vulcan::make_shared_feature_store(registry, store_cfg);
    std::cout << "\n" << prompt() << std::endl;

    std::unordered_map<int, std::string> drives = {
        {1, "/dev/sda"}, // starts good, gets bad
        {2, "/dev/sdb"}  // starts bad, gets good
    };

    for (const auto& d : drives) store->add(ssd{d.first});

    for (int t = 0; t < 5; ++t) {
        auto request = get_request();
        for(const auto& d : drives) {
            if(store->has_listeners(latency)) store->update(latency, ssd{d.first}, get_ssd_latency(d.first, t));
            if(store->has_listeners(temp)) store->update(temp, ssd{d.first}, get_ssd_temp(d.first, t));
        }
        if(store->has_listeners(system_load)) store->update(system_load, get_system_load(t));
        if(store->has_listeners(cpu_usage)) store->update(cpu_usage, get_cpu_usage(t));

        auto candidates = store->all<ssd>();   // every SSD; store->sample<ssd>(k) scores a random subset
        ssd best = *std::ranges::min_element(candidates, {}, [&](ssd s) { return score(*store, s); });

        if(store->has_listeners(chosen_last))
            for(const auto& d : drives) store->update(chosen_last, ssd{d.first}, ssd{d.first} == best);

        dispatch_io(t, best, request);
    }
    return 0;
}
