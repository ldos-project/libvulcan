# Vulcan Python Bindings

A Python wrapper around libvulcan for users who want to write their policy scaffolding (feature declarations, data feeds, decision loop) in Python but still use libvulcan to evolve the heuristic itself. The LLM-authored "EVOLVE block" — listener config, scoring function, comparator — stays in C++ and is compiled into a `.so` that the Python side `dlopen`s at runtime.

## Running the existing examples

1. Run the following commands. This produces `build/libvulcan.so`, the `build/python/vulcan/_vulcan.*.so` extension, and the example plugins under `build/python/examples/`.
  ```bash
  cd ../ # libvulcan root directory
  cmake -B build
  cmake --build build -j
  ```
2. Then run any example:
  ```bash
  LD_LIBRARY_PATH=$PWD/build PYTHONPATH=$PWD/build/python python3 python/examples/rank_example.py
  LD_LIBRARY_PATH=$PWD/build PYTHONPATH=$PWD/build/python python3 python/examples/value_example.py
  LD_LIBRARY_PATH=$PWD/build PYTHONPATH=$PWD/build/python python3 python/examples/value_rank_example.py
  ```

## Adding your own task

To add your own Python-based task to evolve in libVulcan, you need **two files**:

1. **A Python driver** that creates a `FeatureRegistry`, declares all features the policy can use, loads the compiled plugin via `vulcan.load_policy(...)`, creates a `StoreConfig` and a policy config (`RankConfig` or `ValueConfig`) with the task description, calls `plugin.configure_rank(reg, store_cfg, cfg)` or `plugin.configure_value(reg, store_cfg, cfg)` to let the plugin set up listeners and decision functions in one shot, creates a shared feature store with `vulcan.make_shared_feature_store(registry, store_config)`, instantiates the policy with the shared store, then runs the decision loop — feeding observations through the feature store and calling `policy.decide()`. Rank tasks additionally manage the candidate set via `add_object` / `remove_object`.

2. **A C++ plugin** that exports `extern "C"` entry points:
    - `vulcan_configure_rank(registry, store_config, rank_config)` — attaches listeners and sets the scoring function, comparator, and sorting strategy.
    - `vulcan_configure_value(registry, store_config, value_config)` — attaches listeners and sets the value function.

    A rank-only plugin exports `vulcan_configure_rank`. A value-only plugin exports `vulcan_configure_value`. A combined plugin (value+rank sharing a store) exports both.

    Compile with:

    ```bash
    g++ -std=c++20 -O2 -shared -fPIC -I libvulcan/include my_task.cpp -L libvulcan/build -lvulcan -o my_task.so
    ```

    Or add it as a CMake target alongside the existing examples in [`CMakeLists.txt`](../CMakeLists.txt).

3. Run with `LD_LIBRARY_PATH=libvulcan/build PYTHONPATH=libvulcan/build/python python3 my_task.py`.

4. For concrete end-to-end reference, see the bundled examples:
  - Rank: [examples/rank_example.py](examples/rank_example.py) + [examples/rank_policy.cpp](examples/rank_policy.cpp)
  - Value: [examples/value_example.py](examples/value_example.py) + [examples/value_policy.cpp](examples/value_policy.cpp)
  - Rank+Value (shared store): [examples/value_rank_example.py](examples/value_rank_example.py) + [examples/value_rank_policy.cpp](examples/value_rank_policy.cpp)

5. See [API.md](API.md) for the full list of Python-facing types and functions.