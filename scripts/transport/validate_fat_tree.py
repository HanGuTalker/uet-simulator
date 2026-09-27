#!/usr/bin/env python3
"""Validate the deterministic k=4 fat-tree readiness artifacts."""

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
    args = parser.parse_args()

    single = load_json(args.output_dir / "single-single-summary.json")
    all_to_all = load_json(args.output_dir / "alltoall-all-to-all-summary.json")

    require(single["schema_version"] >= 8, "structured-output schema is older than 8")
    require(single["fabric"] == "fat-tree", "single-flow run did not use fat-tree")
    require(single["fat_tree"] == {
        "k": 4,
        "pods": 4,
        "edge_switches": 8,
        "aggregation_switches": 8,
        "core_switches": 4,
    }, "k=4 topology inventory is incorrect")
    require(single["delay_model_ns"]["topology_base_rtt"] == 5100,
            "cross-pod base RTT is not 5100 ns")
    require(single["messages"]["completed"] == 1, "cross-pod message did not complete")
    require(all_to_all["messages"]["expected"] == 240,
            "16-node all-to-all should offer 240 directed messages")
    require(all_to_all["messages"]["completed"] == 240,
            "not every all-to-all message completed")
    require(all_to_all["path_selection"]["active_aggregations"] == 8,
            "all-to-all did not exercise all eight aggregation switches")
    require(all_to_all["path_selection"]["active_cores"] == 4,
            "all-to-all did not exercise all four core switches")

    path_file = args.output_dir / "alltoall-all-to-all-fabric-paths.csv"
    with path_file.open(newline="", encoding="utf-8") as handle:
        paths = list(csv.DictReader(handle))
    cores = {row["to_id"] for row in paths if row["to_tier"] == "core"}
    require(cores == {"0", "1", "2", "3"}, "fabric trace does not contain every core")

    print("PASS k=4 fat-tree: 16 hosts, 8 edge, 8 aggregation, 4 core")
    print("PASS cross-pod single: 1/1 complete, topology base RTT 5.1 us")
    print("PASS all-to-all: 240/240 complete, 8 aggregations and 4 cores active")


if __name__ == "__main__":
    main()
