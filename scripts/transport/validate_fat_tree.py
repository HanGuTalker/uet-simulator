#!/usr/bin/env python3
"""Validate deterministic k-ary fat-tree readiness artifacts."""

import argparse
import csv
import json
from pathlib import Path


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"fat-tree validation failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--k", type=int, default=4)
    args = parser.parse_args()
    k = args.k
    node_count = k**3 // 4
    edge_count = k**2 // 2
    aggregation_count = edge_count
    core_count = k**2 // 4
    message_count = node_count * (node_count - 1)

    single = load_json(args.output_dir / "single-single-summary.json")
    all_to_all = load_json(args.output_dir / "alltoall-all-to-all-summary.json")

    require(single["schema_version"] >= 8, "structured-output schema is older than 8")
    require(single["fabric"] == "fat-tree", "single-flow run did not use fat-tree")
    require(single["fat_tree"] == {
        "k": k,
        "pods": k,
        "edge_switches": edge_count,
        "aggregation_switches": aggregation_count,
        "core_switches": core_count,
    }, f"k={k} topology inventory is incorrect")
    require(single["delay_model_ns"]["topology_base_rtt"] == 5100,
            "cross-pod base RTT is not 5100 ns")
    require(single["messages"]["completed"] == 1, "cross-pod message did not complete")
    require(all_to_all["messages"]["expected"] == message_count,
            f"{node_count}-node all-to-all should offer {message_count} directed messages")
    require(all_to_all["messages"]["completed"] == message_count,
            "not every all-to-all message completed")
    require(all_to_all["path_selection"]["active_aggregations"] == aggregation_count,
            "all-to-all did not exercise every aggregation switch")
    require(all_to_all["path_selection"]["active_cores"] == core_count,
            "all-to-all did not exercise every core switch")

    path_file = args.output_dir / "alltoall-all-to-all-fabric-paths.csv"
    with path_file.open(newline="", encoding="utf-8") as handle:
        paths = list(csv.DictReader(handle))
    cores = {row["to_id"] for row in paths if row["to_tier"] == "core"}
    require(cores == {str(core) for core in range(core_count)},
            "fabric trace does not contain every core")

    print(f"PASS k={k} fat-tree: {node_count} hosts, {edge_count} edge, "
          f"{aggregation_count} aggregation, {core_count} core")
    print("PASS cross-pod single: 1/1 complete, topology base RTT 5.1 us")
    print(f"PASS all-to-all: {message_count}/{message_count} complete, "
          f"{aggregation_count} aggregations and {core_count} cores active")


if __name__ == "__main__":
    main()
