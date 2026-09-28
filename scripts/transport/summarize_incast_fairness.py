#!/usr/bin/env python3
"""Summarize per-flow fairness from one or more workload output directories."""

import argparse
import csv
import math
from pathlib import Path


def read_one(directory: Path, protocol: str) -> dict[str, str | float | int]:
    prefix = directory / f"{protocol}-incast"
    with Path(f"{prefix}-summary.csv").open(newline="", encoding="utf-8") as handle:
        summary = next(csv.DictReader(handle))
    with Path(f"{prefix}-messages.csv").open(newline="", encoding="utf-8") as handle:
        messages = [row for row in csv.DictReader(handle) if int(row["completed"]) == 1]
    rates = [int(row["bytes"]) * 8e9 / int(row["latency_ns"]) for row in messages]
    latencies = [int(row["latency_ns"]) / 1000.0 for row in messages]
    first_message = min(messages, key=lambda row: int(row["message_id"]))
    fastest_message = min(messages, key=lambda row: int(row["latency_ns"]))
    rate_sum = sum(rates)
    jain = rate_sum * rate_sum / (len(rates) * sum(rate * rate for rate in rates))
    mean = sum(latencies) / len(latencies)
    variance = sum((latency - mean) ** 2 for latency in latencies) / len(latencies)
    return {
        "directory": directory.name,
        "protocol": protocol,
        "submission_seed": int(summary.get("submission_seed", 0)),
        "start_jitter_ns": int(summary.get("start_jitter_ns", 0)),
        "goodput_gbps": float(summary["goodput_bps"]) / 1e9,
        "mean_fct_us": mean,
        "min_fct_us": min(latencies),
        "max_fct_us": max(latencies),
        "first_source": int(first_message["source"]),
        "fastest_source": int(fastest_message["source"]),
        "fct_cv": math.sqrt(variance) / mean,
        "jain_goodput": jain,
        "queue_drops": int(summary["queue_dropped_packets"]),
        "retransmissions": int(summary["retransmissions"]),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("directories", nargs="+", type=Path)
    parser.add_argument("--protocol", default="falcon")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    rows = [read_one(directory, args.protocol) for directory in args.directories]
    rows.sort(key=lambda row: (row["start_jitter_ns"], row["submission_seed"]))
    fieldnames = list(rows[0])
    if args.output:
        handle = args.output.open("w", newline="", encoding="utf-8")
    else:
        import sys

        handle = sys.stdout
    try:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    finally:
        if args.output:
            handle.close()


if __name__ == "__main__":
    main()
