#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/uncongested-baseline-800g}
binary=${AI_TRANSPORT_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
protocols=${PROTOCOLS:-"uec rocev2 veroce mrc falcon"}
sizes=${PAYLOAD_SIZES:-"1024 4096 65536 1048576 16777216"}
initial_window_bytes=${INITIAL_WINDOW_BYTES:-65536}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Build it with: ./ns3 build uet-ai-workload-example" >&2
  exit 2
fi

mkdir -p "$output_dir"
raw_summary="$output_dir/raw-summary.csv"
first_summary=1

for bytes in $sizes; do
  size_dir="$output_dir/${bytes}B"
  mkdir -p "$size_dir"
  for protocol in $protocols; do
    prefix="$size_dir/$protocol"
    echo "Running $protocol single-flow baseline with $bytes bytes"
    "$binary" \
      --transport="$protocol" \
      --operation=message \
      --pattern=single \
      --nodes=4 \
      --spines=4 \
      --messages=1 \
      --payloadBytes="$bytes" \
      --fabric=leaf-spine \
      --linkRate=800Gbps \
      --hostLinkDelayNs=100 \
      --fabricLinkDelayNs=250 \
      --switchProcessingDelayNs=250 \
      --nsccBaseRttNs=0 \
      --nsccInitialWindowBytes="$initial_window_bytes" \
      --enableEcn=0 \
      --enableTrimming=0 \
      --outputPrefix="$prefix"

    summary="$prefix-single-summary.csv"
    if (( first_summary )); then
      cp "$summary" "$raw_summary"
      first_summary=0
    else
      tail -n +2 "$summary" >> "$raw_summary"
    fi
  done
done

python3 scripts/transport/summarize_uncongested_baseline.py "$output_dir"
echo "Baseline summary: $output_dir/baseline-summary.csv"
