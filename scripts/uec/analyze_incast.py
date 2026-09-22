#!/usr/bin/env python3
"""Calculate per-sender Incast throughput, latency, and Jain fairness."""

import argparse
import csv
import json
import math
from pathlib import Path


def percentile(values: list[int], fraction: float) -> int:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(fraction * len(ordered)) - 1)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("messages_csv", type=Path)
    parser.add_argument("--output-prefix", type=Path, required=True)
    args = parser.parse_args()

    with args.messages_csv.open(newline="", encoding="utf-8") as source_file:
        rows = list(csv.DictReader(source_file))
    completed = [row for row in rows if row["submitted"] == "1" and row["completed"] == "1"]
    if not completed:
        raise SystemExit("no completed messages")

    grouped: dict[int, list[dict[str, str]]] = {}
    for row in completed:
        grouped.setdefault(int(row["source"]), []).append(row)

    senders = []
    for source, source_rows in sorted(grouped.items()):
        first_submit = min(int(row["submitted_ns"]) for row in source_rows)
        last_completion = max(int(row["completed_ns"]) for row in source_rows)
        makespan = last_completion - first_submit
        byte_count = sum(int(row["bytes"]) for row in source_rows)
        latencies = [int(row["latency_ns"]) for row in source_rows]
        senders.append(
            {
                "source": source,
                "target": int(source_rows[0]["target"]),
                "messages": len(source_rows),
                "completed_bytes": byte_count,
                "first_submit_ns": first_submit,
                "last_completion_ns": last_completion,
                "makespan_ns": makespan,
                "goodput_bps": byte_count * 8.0e9 / makespan,
                "mean_fct_ns": sum(latencies) / len(latencies),
                "p50_fct_ns": percentile(latencies, 0.50),
                "p95_fct_ns": percentile(latencies, 0.95),
                "p99_fct_ns": percentile(latencies, 0.99),
                "max_fct_ns": max(latencies),
            }
        )

    rates = [sender["goodput_bps"] for sender in senders]
    fairness = sum(rates) ** 2 / (len(rates) * sum(rate * rate for rate in rates))
    first_submit = min(sender["first_submit_ns"] for sender in senders)
    last_completion = max(sender["last_completion_ns"] for sender in senders)
    total_bytes = sum(sender["completed_bytes"] for sender in senders)
    aggregate_rate = total_bytes * 8.0e9 / (last_completion - first_submit)

    result = {
        "schema_version": 1,
        "source": str(args.messages_csv),
        "sender_count": len(senders),
        "completed_messages": len(completed),
        "aggregate_goodput_bps": aggregate_rate,
        "jain_fairness": fairness,
        "senders": senders,
    }

    args.output_prefix.parent.mkdir(parents=True, exist_ok=True)
    json_path = args.output_prefix.with_name(args.output_prefix.name + "-sender-summary.json")
    csv_path = args.output_prefix.with_name(args.output_prefix.name + "-sender-summary.csv")
    json_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    with csv_path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=senders[0].keys())
        writer.writeheader()
        writer.writerows(senders)

    print(f"aggregate_goodput_gbps={aggregate_rate / 1e9:.6f}")
    print(f"jain_fairness={fairness:.9f}")
    print(f"outputs={csv_path},{json_path}")


if __name__ == "__main__":
    main()
