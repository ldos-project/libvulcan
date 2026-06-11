"""Python-side scaffolding for the value example.

Mirrors libvulcan/value_example.cpp. The EVOLVE block is compiled from
value_policy.cpp and loaded at runtime.
"""
import os
import random
import vulcan

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
POLICY_SO = os.environ.get(
    "VULCAN_VALUE_POLICY_SO",
    os.path.join(REPO_ROOT, "build", "python", "examples", "value_policy.so"),
)


def main() -> None:
    reg = vulcan.FeatureRegistry()
    rtt = reg.global_features.declare_f64("rtt", "RTT time in milliseconds")
    queue_len = reg.global_features.declare_i64("queue_len", "Queue length in packets")
    prev_cwnd = reg.global_features.declare_f64(
        "prev_cwnd", "Congestion window size returned by the previous decision"
    )

    cfg = vulcan.ValueConfig()
    cfg.set_information(
        "You are building a congestion control policy for a network flow. "
        "Each time the policy is invoked, it should return the next congestion window "
        "(cwnd) size in packets. You have access to RTT measurements, queue length, "
        "and the previous cwnd value to guide your decision."
    )

    plugin = vulcan.load_policy(POLICY_SO)
    plugin.configure_value(reg, cfg)

    policy = vulcan.instantiate_value_policy(reg, cfg)
    print(policy.get_prompt())

    store = policy.feature_store
    random.seed(0)
    for t in range(10):
        store.update(rtt, 50.0 + random.randint(0, 99))
        store.update(queue_len, random.randint(0, 19))
        cwnd = policy.decide()
        store.update(prev_cwnd, cwnd)
        print(f"[t= {t}] cwnd= {cwnd}")


if __name__ == "__main__":
    main()
