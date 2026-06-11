# Vulcan Python API

All symbols are exposed under the top-level `vulcan` package.

## Registry and handles

- `FeatureRegistry()` — create a new empty registry.
- `registry.global_features` — proxy for declaring/looking up global-scoped features.
- `registry.object_features` — proxy for declaring/looking up per-object features.

On either proxy:

- `declare_f64(name: str, description: str) -> HandleF64`
- `declare_i64(name: str, description: str) -> HandleI64`
- `lookup_f64(name: str) -> HandleF64`
- `lookup_i64(name: str) -> HandleI64`

Duplicate declarations throw `RuntimeError`. Lookups throw on missing name, type mismatch, or scope mismatch. `HandleF64` / `HandleI64` are opaque typed references exposing `.id` and `.name`.

## Store config

- `StoreConfig()` — create a new store configuration for listener setup.

Listener configuration is done by the compiled C++ plugin as part of `plugin.configure_rank(...)` or `plugin.configure_value(...)`.

## Configs

- `RankConfig()` — config for a rank task (scoring fn, comparator, sort strategy).
- `ValueConfig()` — config for a value task (value fn).

Both expose:

- `set_information(info: str)` — user-written context describing the task; surfaced in `policy.get_prompt()`.
- `get_information() -> str`

Scoring/value functions, comparators, listeners, and sorting strategies are set by the compiled C++ plugin, not from Python.

## Plugin loader

- `vulcan.load_policy(path: str) -> PolicyPlugin` — dlopens the given `.so`.
- `plugin.configure_rank(registry, store_config, rank_config)` — calls the plugin's `extern "C" vulcan_configure_rank` symbol. Sets up listeners on the store config and configures the scoring function.
- `plugin.configure_value(registry, store_config, value_config)` — calls the plugin's `extern "C" vulcan_configure_value` symbol. Sets up listeners on the store config and configures the value function.

The plugin must export whichever of these entry points matches its policy type. A combined value+rank plugin exports both; the same `StoreConfig` is passed to both calls so listeners accumulate into a single shared store.

## Feature store

- `vulcan.make_shared_feature_store(registry, store_config) -> FeatureStore` — create a feature store from a store config.

The returned store can be shared between multiple policies.

## Policies

- `vulcan.instantiate_rank_policy(registry, rank_config, store) -> RankPolicy`
- `vulcan.instantiate_value_policy(registry, value_config, store) -> ValuePolicy`

Both policies expose:

- `.feature_store -> FeatureStore` — live store reference (same object as passed in).
- `.get_prompt() -> str` — LLM-facing description of the task, features, and listener vocabulary.

`RankPolicy` also has:

- `.add_object(obj_id: int)`
- `.remove_object(obj_id: int)`
- `.decide() -> int` — returns the winning object id, or -1 if no objects.

`ValuePolicy` has:

- `.decide() -> float`

## Feature store updates

`store.update` is overloaded by handle type and arity:

- Global feature: `store.update(handle, value)`
- Per-object feature: `store.update(handle, obj_id, value)`

The handle's declared type (`HandleF64` / `HandleI64`) determines which overload is selected and what value type is accepted.
