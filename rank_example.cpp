#include "vulcan.h"
#include <iostream>
#include <vector>
#include <string>

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

void dispatch_io(int t, int64_t best_id, void* request){
    std::cout << "[t= " << t << "]  Dispatching I/O request to drive: " << best_id << "\n";
}

int main() {
    vulcan::feature_registry registry;
    
    // setup of global + per object features
    auto system_load = registry.global.declare_f64("system_load", "Current system load average");
    auto cpu_usage   = registry.global.declare_f64("cpu_usage", "CPU usage %");
    auto latency = registry.object.declare_f64("latency", "Avg latency (ms)");
    auto temp    = registry.object.declare_i64("temp", "Drive temperature (C)");
    auto prev_decisions = registry.global.declare_i64("prev_decisions", "SSDs chosen for previous decisions.");
    
    vulcan::rank_config config;
    // EVOLVE-BLOCK-START
    config.add_listeners(system_load, {vulcan::listeners::global::RollingWindow(1), vulcan::listeners::global::RollingPercentile(100)});
    config.add_listeners(latency, {vulcan::listeners::object::RollingWindow(5)});
    config.add_listeners(temp, {vulcan::listeners::object::RollingWindow(5)});
    config.add_listeners(prev_decisions, {vulcan::listeners::global::RollingWindow(1)});
    auto scoring_fn = [&](const vulcan::feature_store& fs, int64_t obj_id) -> double {
        double l = fs.get_latest(latency, obj_id);
        double t = fs.get_latest(temp, obj_id);
        auto score = l + t;
        if(fs.get_latest(prev_decisions) != obj_id) return score += 20; // SSD switching overhead
        return score;
    };
    config.set_sorting_function(vulcan::rank::FullSort);
    config.set_scoring_fn(scoring_fn);
    config.set_comparator(vulcan::min);
    // EVOLVE-BLOCK-END

    config.set_information(
        "You are building a policy to dispatch I/O requests to SSDs. " 
        "Whenever you receive an I/O request, this policy will be invoked to decide "
        "which SSD would be the best one to route the request to. You will receive "
        "features such as latency and temps for each SSD as well as some system-wide"
        "features like load, cpu_usage, and which SSDs were chosen for previous requests"
    );
    auto io_ssd_policy = vulcan::instantiate_rank_policy(registry, config);
    std::cout << "\n" << io_ssd_policy.get_prompt() << std::endl;
    vulcan::feature_store& store = io_ssd_policy.get_feature_store();
    
    std::unordered_map<int, std::string> drives = {
        {1, "/dev/sda"}, // starts good, gets bad
        {2, "/dev/sdb"}  // starts bad, gets good
    };

    for (const auto& d : drives) io_ssd_policy.add_object(d.first);
    
    for (int t = 0; t < 5; ++t) {
        // update metadata
        auto request = get_request();
        for(const auto& d : drives) {
            if(config.has_listeners(latency)) store.update(latency, d.first, get_ssd_latency(d.first, t));
            if(config.has_listeners(temp)) store.update(temp, d.first, get_ssd_temp(d.first, t));
        }
        if(config.has_listeners(system_load)) store.update(system_load, get_system_load(t));
        if(config.has_listeners(cpu_usage)) store.update(cpu_usage, get_cpu_usage(t));

        // get decision
        int64_t best_id = vulcan::decision(io_ssd_policy);
        
        // add decision to feature store
        if(config.has_listeners(prev_decisions)) store.update(prev_decisions, best_id);
        

        // implement the decision: use the best_id to send your I/O request to the best drive
        dispatch_io(t, best_id, request);
    }
    return 0;
}