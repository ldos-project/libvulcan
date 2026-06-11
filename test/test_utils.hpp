#pragma once

#include "vulcan.h"
#include <iostream>
#include <exception>
#include <functional>
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
