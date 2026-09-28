#!/usr/bin/env python3
"""Validate a common five-protocol 128-node ECN Incast matrix."""

import argparse
import csv
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"128-node Incast validation failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--protocols", nargs="+", required=True)
    parser.add_argument("--payload-bytes", type=int, required=True)
    args = parser.parse_args()

    with (args.output_dir / "raw-summary.csv").open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    protocols = set(args.protocols)
    require(len(rows) == len(protocols), "protocol matrix is incomplete")
    require({row["protocol"] for row in rows} == protocols,
            "protocol matrix contains a missing or duplicate row")
    for row in rows:
        protocol = row["protocol"]
        require(row["fabric"] == "fat-tree" and int(row["fat_tree_k"]) == 8,
                f"{protocol} used the wrong topology")
        require(int(row["node_count"]) == 128, f"{protocol} used the wrong node count")
        require(int(row["payload_bytes"]) == args.payload_bytes,
                f"{protocol} used the wrong payload")
        require(int(row["expected"]) == 127 and int(row["completed"]) == 127,
                f"{protocol} did not complete all 127 messages")
        require(int(row["ecn_enabled"]) == 1, f"{protocol} did not enable ECN")
        require(int(row["queue_marked_packets"]) > 0, f"{protocol} did not trigger ECN")
        require(int(row["queue_dropped_packets"]) == 0,
                f"{protocol} had queue-disc drops")
        require(int(row["device_queue_drops"]) == 0,
                f"{protocol} had device-queue drops")
        require(0 < int(row["active_aggregations"]) <= 32,
                f"{protocol} reported an invalid aggregation-switch count")
        require(0 < int(row["active_cores"]) <= 16,
                f"{protocol} reported an invalid core-switch count")

    print(f"PASS 128-node ECN Incast: {len(rows)}/{len(protocols)} protocols validated")


if __name__ == "__main__":
    main()
