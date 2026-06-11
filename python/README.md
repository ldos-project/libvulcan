# Vulcan Python Bindings

A Python wrapper around libvulcan for users who want to write their policy scaffolding (feature declarations, data feeds, decision loop) in Python but still use libvulcan to evolve the heuristic itself. The LLM-authored "EVOLVE block" — listener config, scoring function, comparator — stays in C++ and is compiled into a `.so` that the Python side `dlopen`s at runtime.

## Running the existing examples

1. Run the following commands. This produces `build/libvulcan.so`, the `build/python/vulcan/_vulcan.*.so` extension, and the two example plugins `build/python/examples/rank_policy.so` and `value_policy.so`.
  ```bash
  cd ../ # libvulcan root directory
  cmake -B build
  cmake --build build -j
  ```
2. Then run either example:
  ```bash
  LD_LIBRARY_PATH=$PWD/build PYTHONPATH=$PWD/build/python python3 python/examples/rank_example.py
  LD_LIBRARY_PATH=$PWD/build PYTHONPATH=$PWD/build/python python3 python/examples/value_example.py
  ```

## Adding your own task

To add your own Python-based task to evolve in libVulcan, you need **two files**:

1. **A Python driver** that creates a `FeatureRegistry`, declares all features the policy can use, builds a `RankConfig` or `ValueConfig` with the task description, loads the compiled plugin via `vulcan.load_policy(...)` and hands it the registry + config, instantiates the policy, then runs the decision loop — feeding observations through the feature store and calling `policy.decide()`. Rank tasks additionally manage the candidate set via `add_object` / `remove_object`.

2. **A C++ plugin** that exports an `extern "C" vulcan_configure_rank` or `vulcan_configure_value` entry point. It looks up each feature by the same name the Python driver declared, then inside the EVOLVE block attaches listeners and sets the scoring/value function (plus comparator and sorting strategy for rank tasks). Compile with:

    ```bash
    g++ -std=c++20 -O2 -shared -fPIC -I libvulcan/include my_task.cpp -L libvulcan/build -lvulcan -o my_task.so
    ```

    Or add it as a CMake target alongside `rank_policy_example` / `value_policy_example` in [`CMakeLists.txt`](../CMakeLists.txt).

3. Run with `LD_LIBRARY_PATH=libvulcan/build PYTHONPATH=libvulcan/build/python python3 my_task.py`.

4. For concrete end-to-end reference, see the bundled examples:
  - Rank: [examples/rank_example.py](examples/rank_example.py) + [examples/rank_policy.cpp](examples/rank_policy.cpp)
  - Value: [examples/value_example.py](examples/value_example.py) + [examples/value_policy.cpp](examples/value_policy.cpp)

5. See [API.md](API.md) for the full list of Python-facing types and functions.