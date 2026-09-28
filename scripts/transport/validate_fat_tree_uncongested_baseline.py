#!/usr/bin/env python3
"""Validate the five-protocol cross-pod fat-tree baseline matrix."""

import argparse
import csv
from pathlib import Path


PROTOCOLS = {"uec", "rocev2", "veroce", "mrc", "falcon"}
PAYLOAD_BYTES = {2097152, 16777216, 67108864, 134217728}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"fat-tree baseline validation failed: {message}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    with (args.output_dir / "raw-summary.csv").open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    require(len(rows) == len(PROTOCOLS) * len(PAYLOAD_BYTES), "matrix is not 5 x 4")
    observed = {(row["protocol"], int(row["payload_bytes"])) for row in rows}
    expected = {(protocol, size) for protocol in PROTOCOLS for size in PAYLOAD_BYTES}
    require(observed == expected, "protocol/payload matrix contains missing or duplicate entries")

    for row in rows:
        label = f'{row["protocol"]}/{row["payload_bytes"]}B'
        require(row["fabric"] == "fat-tree", f"{label} used the wrong fabric")
        require(int(row["fat_tree_k"]) == 4, f"{label} used the wrong fat-tree k")
        require(int(row["node_count"]) == 16, f"{label} used the wrong endpoint count")
        require(int(row["link_rate_bps"]) == 800_000_000_000,
                f"{label} used the wrong link rate")
        require(int(row["topology_base_rtt_ns"]) == 5100,
                f"{label} used the wrong topology RTT")
        require(int(row["completed"]) == int(row["expected"]) == 1,
                f"{label} did not complete")
        require(int(row["retransmissions"]) == 0, f"{label} retransmitted")
        require(int(row["timeouts"]) == 0, f"{label} timed out")
        require(int(row["nacks"]) == 0, f"{label} received a NACK")
        require(int(row["queue_marked_packets"]) == 0, f"{label} was ECN-marked")
        require(int(row["queue_dropped_packets"]) == 0, f"{label} had a queue-disc drop")
        require(int(row["device_queue_drops"]) == 0, f"{label} had a device-queue drop")

    print("PASS fat-tree no-congestion baseline: 20/20 configurations validated")


if __name__ == "__main__":
    main()
