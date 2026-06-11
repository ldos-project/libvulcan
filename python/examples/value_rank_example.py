"""Python-side scaffolding for the value+rank example.

Demonstrates sharing a single feature store between a value policy and a
rank policy. Stage 1 (value) decides whether to issue a READ; stage 2 (rank)
picks which SSD to target.
"""
import os
import vulcan

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
POLICY_SO = os.environ.get(
    "VULCAN_VALUE_RANK_POLICY_SO",
    os.path.join(REPO_ROOT, "build", "python", "examples", "value_rank_policy.so"),
)


def get_io_queue_depth(t: int) -> float:
    return (80.0 + t * 5.0) if t % 2 == 0 else (20.0 - t * 2.0)


def get_system_iops(t: int) -> float:
    return 1000.0 - t * 100.0


def get_ssd_latency(ssd_id: int, t: int) -> float:
    if ssd_id == 1:
        return 5.0 + t * 15.0
    elif ssd_id == 2:
        return 50.0 - t * 5.0
    else:
        return 30.0 + (t % 3) * 10.0


def get_ssd_queue_len(ssd_id: int, t: int) -> int:
    if ssd_id == 1:
        return 2 + t * 3
    elif ssd_id == 2:
        return 15 - t * 2
    else:
        return 8 + (t % 2) * 4


def main() -> None:
    reg = vulcan.FeatureRegistry()

    io_queue_depth = reg.global_features.declare_f64(
        "io_queue_depth", "Pending I/O requests in the system queue"
    )
    system_iops = reg.global_features.declare_f64(
        "system_iops", "Current system-wide IOPS throughput"
    )
    ssd_latency = reg.object_features.declare_f64(
        "ssd_latency", "Read latency in ms for this SSD"
    )
    ssd_queue_len = reg.object_features.declare_i64(
        "ssd_queue_len", "Pending requests queued for this SSD"
    )

    plugin = vulcan.load_policy(POLICY_SO)

    # One store_config accumulates listeners from both configure calls.
    store_cfg = vulcan.StoreConfig()

    vcfg = vulcan.ValueConfig()
    vcfg.set_information(
        "Stage 1: decide whether to issue a READ now or WAIT based on system-wide "
        "I/O queue depth and IOPS headroom."
    )
    plugin.configure_value(reg, store_cfg, vcfg)

    rcfg = vulcan.RankConfig()
    rcfg.set_information(
        "Stage 2: if READ is chosen, select the best SSD based on per-drive latency "
        "and queue length."
    )
    plugin.configure_rank(reg, store_cfg, rcfg)

    # Shared store — both policies see the same data.
    store = vulcan.make_shared_feature_store(reg, store_cfg)
    value_pol = vulcan.instantiate_value_policy(reg, vcfg, store)
    rank_pol = vulcan.instantiate_rank_policy(reg, rcfg, store)

    ssds = [1, 2, 3]
    for ssd_id in ssds:
        rank_pol.add_object(ssd_id)

    print(value_pol.get_prompt())
    print(rank_pol.get_prompt())

    for t in range(7):
        store.update(io_queue_depth, get_io_queue_depth(t))
        store.update(system_iops, get_system_iops(t))
        for ssd_id in ssds:
            store.update(ssd_latency, ssd_id, get_ssd_latency(ssd_id, t))
            store.update(ssd_queue_len, ssd_id, get_ssd_queue_len(ssd_id, t))

        action = value_pol.decide()
        if action < 0.5:
            print(f"[t={t}]  Decision: WAIT (queue saturated / low IOPS)")
        else:
            best_ssd = rank_pol.decide()
            print(f"[t={t}]  Decision: READ -> SSD {best_ssd}")


if __name__ == "__main__":
    main()
