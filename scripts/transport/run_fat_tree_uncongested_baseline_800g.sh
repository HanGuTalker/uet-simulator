#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/fat-tree-uncongested-baseline-800g}
binary=${AI_TRANSPORT_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
protocols=${PROTOCOLS:-"uec rocev2 veroce mrc falcon"}
sizes=${PAYLOAD_SIZES:-"2097152 16777216 67108864 134217728"}
jobs=${JOBS:-1}
# An 800 Gbps path with a topology-derived 5.1 us RTT has a 510,000-byte BDP.
# Use the same 1.5-BDP initial window for every transport adapter.
initial_window_bytes=${INITIAL_WINDOW_BYTES:-765000}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Build it with: ./ns3 build uet-ai-workload-example" >&2
  exit 2
fi
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
  echo "JOBS must be a positive integer: $jobs" >&2
  exit 2
fi

mkdir -p "$output_dir"
raw_summary="$output_dir/raw-summary.csv"

run_one() {
  local protocol=$1
  local bytes=$2
  local size_dir="$output_dir/${bytes}B"
  local prefix="$size_dir/$protocol"
  mkdir -p "$size_dir"
  echo "Running $protocol cross-pod fat-tree baseline with $bytes bytes"
  "$binary" \
    --transport="$protocol" \
    --operation=message \
    --pattern=single \
    --nodes=16 \
    --messages=1 \
    --payloadBytes="$bytes" \
    --fabric=fat-tree \
    --fatTreeK=4 \
    --linkRate=800Gbps \
    --hostLinkDelayNs=100 \
    --fabricLinkDelayNs=250 \
    --switchProcessingDelayNs=250 \
    --nsccBaseRttNs=0 \
    --nsccInitialWindowBytes="$initial_window_bytes" \
    --enableEcn=0 \
    --enableTrimming=0 \
    --outputPrefix="$prefix"
}

wait_batch() {
  local failed=0
  local pid
  for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
      failed=1
    fi
  done
  pids=()
  if (( failed )); then
    echo "At least one fat-tree baseline run failed" >&2
    return 1
  fi
}

pids=()
for bytes in $sizes; do
  for protocol in $protocols; do
    run_one "$protocol" "$bytes" &
    pids+=("$!")
    if (( ${#pids[@]} >= jobs )); then
      wait_batch
    fi
  done
done
if (( ${#pids[@]} > 0 )); then
  wait_batch
fi

first_summary=1
for bytes in $sizes; do
  for protocol in $protocols; do
    summary="$output_dir/${bytes}B/$protocol-single-summary.csv"
    if (( first_summary )); then
      cp "$summary" "$raw_summary"
      first_summary=0
    else
      tail -n +2 "$summary" >> "$raw_summary"
    fi
  done
done

python3 scripts/transport/summarize_uncongested_baseline.py "$output_dir"
python3 scripts/transport/validate_fat_tree_uncongested_baseline.py "$output_dir" \
  --protocols $protocols --sizes $sizes
echo "Fat-tree baseline summary: $output_dir/baseline-summary.csv"
