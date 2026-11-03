#pragma once

#include "vulcan.h"
#include <iostream>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>

// Simple assertion macro
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << msg << "\n"; \
        std::exit(1); \
    }

namespace vulcan_test {

// Common setup helper

// ANSI Colors
#define GREEN "\033[32m"
#define RED   "\033[31m"
#define RESET "\033[0m"

// True if building a store from this registry and config is rejected, e.g. a listener on a value type it
// does not accept
inline bool construction_throws(const vulcan::feature_registry& registry, const vulcan::store_config& config) {
    try {
        auto features = vulcan::instantiate_feature_store(registry, config);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

inline void run_test(const std::string& name, std::function<void(void)> test_func) {
    try {
        test_func();
        std::cout << "[" << GREEN << "✓" << RESET << "] " << name << "\n";
    } catch (const std::exception& e) {
        std::cout << "[" << RED << "✗" << RESET << "] " << name << " - Failed: " << e.what() << "\n";
        exit(1);
    } catch (...) {
        std::cout << "[" << RED << "✗" << RESET << "] " << name << " - Failed: Unknown error\n";
        exit(1);
    }
}
}
