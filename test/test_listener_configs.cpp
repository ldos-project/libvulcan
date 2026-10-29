#include "test_utils.hpp"
#include <iostream>

int main() {
    vulcan_test::run_test("Listener configs - to_string prints the code that creates them", []() {
        namespace L = vulcan::listeners;
        TEST_ASSERT(L::Average().to_string() == "vulcan::listeners::Average()", "Average");
        TEST_ASSERT(L::MinMax().to_string() == "vulcan::listeners::MinMax()", "MinMax");
        TEST_ASSERT(L::RollingWindow(5).to_string() == "vulcan::listeners::RollingWindow(5)", "RollingWindow");
        TEST_ASSERT(L::RollingPercentile(100).to_string() == "vulcan::listeners::RollingPercentile(100)", "RollingPercentile");
        TEST_ASSERT(L::RollingCount(32).to_string() == "vulcan::listeners::RollingCount(32)", "RollingCount");
        TEST_ASSERT(L::EWMA({0.25, 0.05}).to_string() == "vulcan::listeners::EWMA({0.25, 0.05})", "EWMA");
        TEST_ASSERT(L::PopulationPercentile().to_string() == "vulcan::listeners::PopulationPercentile()", "PopulationPercentile");
    });
    return 0;
}
