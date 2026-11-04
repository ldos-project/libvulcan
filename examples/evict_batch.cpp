// Batch cache eviction: when the cache is full, evict several pages at once.
//
// Output type: std::set<page>. A set holds each member at most once, so the mechanism never evicts the same
// page twice; the batch size is the function's choice (Anvil states a bound such as "at most 2" as a
// contract on the function).

#include "vulcan.h"
#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <string>

// ---- Trusted compute base: groups, features, and the decision's prototype ----
VULCAN_GROUP(page, "Resident cache entries");

vulcan::feature_registry registry = [] {
    vulcan::feature_registry r;
    r.declare_group<page>();
    return r;
}();

const vulcan::feature_handle<int64_t, page> last_access =
    registry.declare<int64_t, page>("last_access", "Tick of this page's most recent access", 0);
const vulcan::feature_handle<int64_t, page> accesses =
    registry.declare<int64_t, page>("accesses", "Accesses since this page entered the cache", 1);

std::set<page> evict_batch(const vulcan::feature_store& fs);

// ---- The developer's prompt: the task, what the policy can read, and the functions to write ----
std::string prompt() {
    return "--- Task ---\nThe cache is full. Choose which resident pages to evict to make room.\n\n" +
           registry.describe() +
           "--- Write ---\n"
           "1. void configure(vulcan::store_config& listeners): attach the listeners your function reads.\n"
           "2. std::set<page> evict_batch(const vulcan::feature_store& fs)\n"
           "   Return the pages to evict, at most 2.\n";
}

// ---- Listeners the mechanism reads itself ----
void guaranteed_listeners(vulcan::store_config& listeners) {
    listeners.add_listener(accesses, vulcan::listeners::RollingWindow(1));
}

// ---- Synthesized by the LLM ----
// EVOLVE-BLOCK-START
void configure(vulcan::store_config& listeners) {
    listeners.add_listener(last_access, vulcan::listeners::RollingWindow(1));
}

// Evict the two least recently used pages among those accessed at most twice
std::set<page> evict_batch(const vulcan::feature_store& fs) {
    std::set<page> victims;
    for (int i = 0; i < 2; ++i) {
        std::optional<page> oldest;
        int64_t oldest_tick = 0;
        for (page p : fs.all<page>()) {
            if (victims.contains(p) || fs.get_latest(accesses, p) > 2) continue;
            int64_t tick = fs.get_latest(last_access, p);
            if (!oldest || tick < oldest_tick) { oldest = p; oldest_tick = tick; }
        }
        if (oldest) victims.insert(*oldest);
    }
    return victims;
}
// EVOLVE-BLOCK-END

// ---- Mechanism: a cache of 8 pages over a deterministic trace of 20 distinct pages ----
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--prompt") {
        std::cout << prompt();
        return 0;
    }
    vulcan::store_config listeners;
    guaranteed_listeners(listeners);
    configure(listeners);
    auto store = vulcan::instantiate_feature_store(registry, listeners);

    const std::size_t capacity = 8;
    uint32_t state = 7;
    int hits = 0, requests = 60;
    for (int64_t tick = 0; tick < requests; ++tick) {
        state = state * 1103515245u + 12345u;
        page p{(state >> 16) % 20 < 12 ? (state >> 16) % 6 : (state >> 16) % 20};   // pages 0-5 are hot

        if (store.contains(p)) {
            ++hits;
            store.update(last_access, p, tick);
            store.update(accesses, p, store.get_latest(accesses, p) + 1);
            continue;
        }
        if (store.count<page>() >= capacity) {
            std::set<page> victims = evict_batch(store);
            if (victims.empty()) victims.insert(*store.all<page>().begin());   // the policy declined: evict any page
            std::cout << "[tick " << tick << "] evict";
            for (page v : victims) {
                std::cout << " " << static_cast<int64_t>(v);
                store.remove(v);
            }
            std::cout << "\n";
        }
        store.add(p, {{last_access, tick}});
    }
    std::cout << "Hit ratio: " << hits << "/" << requests << "\n";
    return 0;
}
