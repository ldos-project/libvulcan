# LibVulcan

LibVulcan is a feature registry and decision SDK designed to bridge human-defined scaffolding with LLM-synthesized policies.

## Interfaces

### 1. Human <-> Vulcan (Scaffolding)
Humans act as the architects who define the environment and capabilities.
- **Declare Features**: Use `feature_registry::declare_f64` or `declare_i64` to register observable metrics (e.g., RTT, Queue Length).
- **Configure Listeners**: Use `add_listeners` to attach processors to features.
    - `RollingWindowProcessor(window)`: Tracks Min, Max, and Avg over a sliding window.
    - `HistoryArray(window)`: Keeps a raw history of recent values.
- **Instantiate Store**: Create a `feature_store` which manages the runtime state of these listeners.
- **Set Policy**: Assign a lambda function to `set_value_fn` that implements the decision logic using the store.

Example:
```cpp
vulcan::feature_registry registry;
auto rtt = registry.declare_f64("rtt", "Round Trip Time");
rtt.add_listeners({vulcan::RollingWindowProcessor(10)});

auto features = vulcan::instantiate_feature_store(registry);
```

### 2. Vulcan <-> LLM (Evolve Block)
The LLM acts as the "Policy Synthesizer". It writes the decision logic inside the `EVOLVE-BLOCK`. To help the LLM understand what data is available, Vulcan provides `registry.generate_policy_prompt()`. This outputs a structured description of all registered features and their enabled accessors (based on attached listeners).

The LLM output (the code between `EVOLVE-BLOCK-START` and `EVOLVE-BLOCK-END`) should:
- Use the provided `feature_store& fs`.
- Access feature values using the appropriate accessors (e.g., `fs.get_max(rtt)`).
- Return a decision value (typically `double`).

**Generated Prompt Example:**
```text
Available Features:
- rtt: RTT time in milliseconds
    RollingWindowProcessor (window=3):
      - get_max(handle): Returns maximum value
      - get_min(handle): Returns minimum value
      - get_avg(handle): Returns average value(handle)
...
```