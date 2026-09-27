#!/usr/bin/env python3
"""Create the compact no-congestion comparison table from workload outputs."""

from __future__ import annotations

import csv
import sys
from pathlib import Path


PROTOCOL_ORDER = {name: index for index, name in enumerate(("uec", "rocev2", "veroce", "mrc", "falcon"))}


def fabric_wire_bytes(path: Path) -> int:
    with path.open(newline="", encoding="utf-8") as stream:
        rows = csv.DictReader(stream)
        return sum(
            int(row["wire_bytes"])
            for row in rows
            if "wire_accounting" not in row or row["wire_accounting"] == "1"
        )


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUTPUT_DIRECTORY", file=sys.stderr)
        return 2

    output_dir = Path(sys.argv[1])
    rows: list[dict[str, object]] = []
    for summary_path in output_dir.glob("*B/*-single-summary.csv"):
        with summary_path.open(newline="", encoding="utf-8") as stream:
            summary = next(csv.DictReader(stream))
        protocol = summary["protocol"]
        payload_bytes = int(summary["payload_bytes"])
        completed_payload_bytes = int(summary["completed_payload_bytes"])
        wire_bytes = fabric_wire_bytes(
            summary_path.with_name(summary_path.name.replace("-summary.csv", "-fabric-paths.csv"))
        )
        rows.append(
            {
                "protocol": protocol,
                "payload_bytes": payload_bytes,
                "expected": int(summary["expected"]),
                "completed": int(summary["completed"]),
                "completion_rate": summary["completion_rate"],
                "goodput_bps": summary["goodput_bps"],
                "mean_latency_ns": summary["mean_latency_ns"],
                "p50_latency_ns": summary["p50_latency_ns"],
                "p95_latency_ns": summary["p95_latency_ns"],
                "p99_latency_ns": summary["p99_latency_ns"],
                "max_latency_ns": summary["max_latency_ns"],
                "tx_datagrams": int(summary["tx_datagrams"]),
                "retransmissions": int(summary["retransmissions"]),
                "timeouts": int(summary["timeouts"]),
                "nacks": int(summary["nacks"]),
                "active_spines": int(summary["active_spines"]),
                "active_aggregations": int(summary.get("active_aggregations", 0)),
                "active_cores": int(summary.get("active_cores", 0)),
                "fabric_path_packets": int(summary["fabric_path_packets"]),
                "fabric_wire_bytes": wire_bytes,
                "fabric_wire_bytes_per_payload_byte": (
                    f"{wire_bytes / completed_payload_bytes:.9f}"
                    if completed_payload_bytes
                    else ""
                ),
            }
        )

    rows.sort(key=lambda row: (int(row["payload_bytes"]), PROTOCOL_ORDER[str(row["protocol"])]))
    if not rows:
        print(f"no baseline summaries found below {output_dir}", file=sys.stderr)
        return 1

    output_path = output_dir / "baseline-summary.csv"
    with output_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
