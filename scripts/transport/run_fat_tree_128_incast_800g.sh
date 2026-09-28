#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/fat-tree-128-incast-800g}
binary=${AI_TRANSPORT_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
protocols=${PROTOCOLS:-"uec rocev2 veroce mrc falcon"}
jobs=${JOBS:-5}
payload_bytes=${PAYLOAD_BYTES:-65536}
# Holds one 4096-byte payload plus the largest modeled protocol headers without
# making the synchronized first-RTT burst exceed the configured 512 KiB queue.
initial_window_bytes=${INITIAL_WINDOW_BYTES:-4608}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
  echo "JOBS must be a positive integer: $jobs" >&2
  exit 2
fi

mkdir -p "$output_dir"

run_one() {
  local protocol=$1
  local prefix="$output_dir/$protocol"
  echo "Running $protocol 127-to-1 ECN Incast with $payload_bytes bytes per sender"
  "$binary" \
    --transport="$protocol" \
    --operation=message \
    --pattern=incast \
    --nodes=128 \
    --messages=1 \
    --payloadBytes="$payload_bytes" \
    --fabric=fat-tree \
    --fatTreeK=8 \
    --linkRate=800Gbps \
    --hostLinkDelayNs=100 \
    --fabricLinkDelayNs=250 \
    --switchProcessingDelayNs=250 \
    --startGapNs=0 \
    --nsccBaseRttNs=0 \
    --nsccTargetQueueDelayNs=800 \
    --nsccInitialWindowBytes="$initial_window_bytes" \
    --queuePackets=10000 \
    --enableEcn=1 \
    --enableTrimming=0 \
    --ecnMinBytes=65536 \
    --ecnMaxBytes=98304 \
    --ecnQueueLimitBytes=524288 \
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
    echo "At least one 128-node Incast run failed" >&2
    return 1
  fi
}

pids=()
for protocol in $protocols; do
  run_one "$protocol" &
  pids+=("$!")
  if (( ${#pids[@]} >= jobs )); then
    wait_batch
  fi
done
if (( ${#pids[@]} > 0 )); then
  wait_batch
fi

raw_summary="$output_dir/raw-summary.csv"
first_summary=1
for protocol in $protocols; do
  summary="$output_dir/$protocol-incast-summary.csv"
  if (( first_summary )); then
    cp "$summary" "$raw_summary"
    first_summary=0
  else
    tail -n +2 "$summary" >> "$raw_summary"
  fi
done

python3 scripts/transport/validate_fat_tree_128_incast.py "$output_dir" \
  --protocols $protocols --payload-bytes "$payload_bytes"
echo "128-node Incast summary: $raw_summary"
