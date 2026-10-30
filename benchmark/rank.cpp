#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>

#include "vulcan.h"

VULCAN_GROUP(cached_obj, "Cached objects");

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
    registry.declare_group<cached_obj>();
    auto f_1 = registry.declare<double, cached_obj>("f_1", "Feature 1");
    auto f_2 = registry.declare<double, cached_obj>("f_2", "Feature 2");
    auto f_3 = registry.declare<double, cached_obj>("f_3", "Feature 3");
    auto f_4 = registry.declare<double, cached_obj>("f_4", "Feature 4");

    vulcan::store_config store_cfg;
    vulcan::rank_config<cached_obj> config;

    if (!disable_listeners) {
        store_cfg.add_listeners(f_1, {vulcan::listeners::RollingWindow(1)});
        store_cfg.add_listeners(f_2, {vulcan::listeners::RollingWindow(1)});
        store_cfg.add_listeners(f_3, {vulcan::listeners::RollingWindow(1)});
        store_cfg.add_listeners(f_4, {vulcan::listeners::RollingWindow(1)});
    }

    auto scoring_fn = [&](const vulcan::feature_store& fs, cached_obj o) -> double {
        if (disable_listeners) {
            return 0.0;   // no features to read; measures ranking cost alone
        }
        double val_4 = fs.get_latest(f_4, o);
        double val_3 = fs.get_latest(f_3, o);
        return val_4 + val_3;
    };

    config.set_sorting_function(vulcan::rank::SampleSort);
    config.set_scoring_fn(scoring_fn);
    config.set_comparator(vulcan::min);

    auto store = vulcan::make_shared_feature_store(registry, store_cfg);
    auto policy = std::make_unique<vulcan::rank_policy<cached_obj>>(vulcan::instantiate_rank_policy(registry, config, store));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> obj_dist(1, TOTAL_OBJECTS * 2);
    std::uniform_real_distribution<double> lat_dist(1.0, 100.0);
    std::uniform_real_distribution<double> size_dist(100.0, 10000.0);

    int num_objects = 0;
    std::vector<int> current_objects;
    current_objects.reserve(TOTAL_OBJECTS);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TOTAL_NUM_REQUESTS; ++i) {
        cached_obj o{obj_dist(rng)};

        if (num_objects >= TOTAL_OBJECTS) {
            cached_obj victim = *vulcan::decision(*policy);
            store->remove(victim);
            num_objects--;
        }

        store->add(o);
        store->update(f_1, o, lat_dist(rng));
        store->update(f_2, o, size_dist(rng));
        store->update(f_3, o, static_cast<double>(i));
        store->update(f_4, o, 1.0);

        num_objects++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    std::cout << "Benchmark completed in " << duration.count() << " seconds." << std::endl;
    std::cout << "Throughput: " << TOTAL_NUM_REQUESTS / duration.count() << " ops/sec." << std::endl;

    return 0;
}
