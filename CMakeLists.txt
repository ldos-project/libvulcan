cmake_minimum_required(VERSION 3.15)
project(vulcan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(vulcan SHARED
    src/vulcan.cpp
    src/feature_registry.cpp
    src/feature_store.cpp
)
target_include_directories(vulcan PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Python bindings
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
        OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _pybind_rc
    )
    if(_pybind_rc EQUAL 0)
        list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_CMAKE_DIR}")
        find_package(pybind11 CONFIG REQUIRED)
    else()
        message(FATAL_ERROR "pybind11 not found. Install with: pip install pybind11")
    endif()
endif()

pybind11_add_module(_vulcan python/vulcan_pybind.cpp)
target_link_libraries(_vulcan PRIVATE vulcan)
set_target_properties(_vulcan PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python/vulcan
)

# Copy Python package __init__.py next to the built extension
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/vulcan/__init__.py
    ${CMAKE_BINARY_DIR}/python/vulcan/__init__.py
    COPYONLY
)

# LLM-authored example policy plugins (compiled to .so, dlopen'd at runtime).
# These are the "EVOLVE block" artifacts.
add_library(rank_policy_example SHARED python/examples/rank_policy.cpp)
target_link_libraries(rank_policy_example PRIVATE vulcan)
set_target_properties(rank_policy_example PROPERTIES
    PREFIX ""
    OUTPUT_NAME "rank_policy"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python/examples
)

add_library(value_policy_example SHARED python/examples/value_policy.cpp)
target_link_libraries(value_policy_example PRIVATE vulcan)
set_target_properties(value_policy_example PROPERTIES
    PREFIX ""
    OUTPUT_NAME "value_policy"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python/examples
)

add_library(value_rank_policy_example SHARED python/examples/value_rank_policy.cpp)
target_link_libraries(value_rank_policy_example PRIVATE vulcan)
set_target_properties(value_rank_policy_example PROPERTIES
    PREFIX ""
    OUTPUT_NAME "value_rank_policy"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python/examples
)
