"""Python-side scaffolding for the rank example.

Mirrors libvulcan/rank_example.cpp. The EVOLVE block is compiled from
rank_policy.cpp and loaded at runtime.
"""
import os
import vulcan

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
POLICY_SO = os.environ.get(
    "VULCAN_RANK_POLICY_SO",
    os.path.join(REPO_ROOT, "build", "python", "examples", "rank_policy.so"),
)


def get_ssd_latency(ssd_id: int, t: int) -> float:
    return 10.0 + t * 30.0 if ssd_id == 1 else 150.0 - t * 30.0


def get_ssd_temp(ssd_id: int, t: int) -> int:
    return 30 + t * 10 if ssd_id == 1 else 90 - t * 10


def main() -> None:
    reg = vulcan.FeatureRegistry()

    reg.global_features.declare_f64("system_load", "Current system load average")
    reg.global_features.declare_f64("cpu_usage", "CPU usage %")
    latency = reg.object_features.declare_f64("latency", "Avg latency (ms)")
    temp = reg.object_features.declare_i64("temp", "Drive temperature (C)")
    prev_decisions = reg.global_features.declare_i64(
        "prev_decisions", "SSDs chosen for previous decisions."
    )
    system_load = reg.global_features.lookup_f64("system_load")

    plugin = vulcan.load_policy(POLICY_SO)

    store_cfg = vulcan.StoreConfig()
    cfg = vulcan.RankConfig()
    cfg.set_information(
        "You are building a policy to dispatch I/O requests to SSDs. "
        "Whenever you receive an I/O request, this policy will be invoked to decide "
        "which SSD would be the best one to route the request to. You will receive "
        "features such as latency and temps for each SSD as well as some system-wide "
        "features like load, cpu_usage, and which SSDs were chosen for previous requests."
    )
    plugin.configure_rank(reg, store_cfg, cfg)

    store = vulcan.make_shared_feature_store(reg, store_cfg)
    policy = vulcan.instantiate_rank_policy(reg, cfg, store)
    print(policy.get_prompt())

    drives = {1: "/dev/sda", 2: "/dev/sdb"}
    for d in drives:
        policy.add_object(d)

    for t in range(5):
        for d in drives:
            store.update(latency, d, get_ssd_latency(d, t))
            store.update(temp, d, get_ssd_temp(d, t))
        store.update(system_load, t * 10.0)

        best = policy.decide()
        store.update(prev_decisions, best)
        print(f"[t= {t}]  Dispatching I/O request to drive: {best}")


if __name__ == "__main__":
    main()
