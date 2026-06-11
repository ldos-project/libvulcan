#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>

#include "vulcan.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <N_OBJECTS> <K_OPERATIONS>\n";
        return 1;
    }

    const int TOTAL_OBJECTS = std::stoi(argv[1]);
    const int TOTAL_NUM_REQUESTS = std::stoi(argv[2]);

    bool disable_listeners = false;
    for (int i = 3; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-listeners") {
            disable_listeners = true;
        }
    }

    std::cout << "Running benchmark with TOTAL_OBJECTS=" << TOTAL_OBJECTS << " and TOTAL_NUM_REQUESTS=" << TOTAL_NUM_REQUESTS << (disable_listeners ? " (Listeners disabled)" : "") << std::endl;

    vulcan::feature_registry registry;
    auto f_1 = registry.object.declare_f64("f_1", "Feature 1");
    auto f_2 = registry.object.declare_f64("f_2", "Feature 2");
    auto f_3 = registry.object.declare_f64("f_3", "Feature 3");
    auto f_4 = registry.object.declare_f64("f_4", "Feature 4");

    vulcan::rank_config config;
    if (!disable_listeners) {
        config.add_listeners(f_1, {vulcan::listeners::object::RollingWindow(1)});
        config.add_listeners(f_2, {vulcan::listeners::object::RollingWindow(1)});
        config.add_listeners(f_3, {vulcan::listeners::object::RollingWindow(1)});
        config.add_listeners(f_4, {vulcan::listeners::object::RollingWindow(1)});
    }

    auto scoring_fn = [&](const vulcan::feature_store& fs, int64_t obj_id) -> double {
        if (disable_listeners) {
            return static_cast<double>(obj_id);
        }
        double val_4 = fs.get_latest(f_4, obj_id);
        double val_3 = fs.get_latest(f_3, obj_id);
        return val_4 + val_3; // Simple priority
    };

    config.set_sorting_function(vulcan::rank::SampleSort);
    config.set_scoring_fn(scoring_fn);
    config.set_comparator(vulcan::min); // Evict lowest score

    auto policy = std::make_unique<vulcan::rank_policy>(vulcan::instantiate_rank_policy(registry, config));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> obj_dist(1, TOTAL_OBJECTS * 2);
    std::uniform_real_distribution<double> lat_dist(1.0, 100.0);
    std::uniform_real_distribution<double> size_dist(100.0, 10000.0);

    int num_objects = 0;
    std::vector<int> current_objects;
    current_objects.reserve(TOTAL_OBJECTS);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TOTAL_NUM_REQUESTS; ++i) {
        int64_t obj_id = obj_dist(rng);

        if (num_objects >= TOTAL_OBJECTS) {
            int64_t victim_id = vulcan::decision(*policy);
            policy->remove_object(victim_id);
            num_objects--;
        }

        policy->add_object(obj_id);
        policy->get_feature_store().update(f_1, obj_id, lat_dist(rng));
        policy->get_feature_store().update(f_2, obj_id, size_dist(rng));
        policy->get_feature_store().update(f_3, obj_id, static_cast<double>(i));
        policy->get_feature_store().update(f_4, obj_id, 1.0);

        num_objects++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    std::cout << "Benchmark completed in " << duration.count() << " seconds." << std::endl;
    std::cout << "Throughput: " << TOTAL_NUM_REQUESTS / duration.count() << " ops/sec." << std::endl;

    return 0;
}
