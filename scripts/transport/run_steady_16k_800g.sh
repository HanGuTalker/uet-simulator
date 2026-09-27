#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/steady-16k-2mib-800g}
binary=${AI_TRANSPORT_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
protocols=${PROTOCOLS:-"uec rocev2 veroce mrc falcon"}
total_bytes=${TOTAL_BYTES:-2097152}
payload_bytes=16384

if (( total_bytes == 0 || total_bytes % payload_bytes != 0 )); then
  echo "TOTAL_BYTES must be a nonzero multiple of $payload_bytes" >&2
  exit 2
fi
message_count=$((total_bytes / payload_bytes))

# The workload interleaves occurrence slots across four endpoints, so 41 ns
# becomes a 164 ns interval for the selected single source. This is the ideal
# payload serialization interval for 16 KiB at 800 Gbps, rounded to 1 ns.
start_gap_ns=41

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi

mkdir -p "$output_dir/16384B"

for protocol in $protocols; do
  prefix="$output_dir/16384B/$protocol"
  echo "Running $protocol: $message_count x $payload_bytes bytes ($total_bytes bytes total)"
  "$binary" \
    --transport="$protocol" \
    --operation=message \
    --pattern=single \
    --nodes=4 \
    --spines=4 \
    --messages="$message_count" \
    --payloadBytes="$payload_bytes" \
    --fabric=leaf-spine \
    --linkRate=800Gbps \
    --hostLinkDelayNs=100 \
    --fabricLinkDelayNs=250 \
    --switchProcessingDelayNs=250 \
    --reusePdc=1 \
    --startGapNs="$start_gap_ns" \
    --nsccBaseRttNs=0 \
    --nsccInitialWindowBytes=465000 \
    --enableEcn=0 \
    --enableTrimming=0 \
    --outputPrefix="$prefix"
done

python3 scripts/transport/summarize_uncongested_baseline.py "$output_dir"
echo "Steady 16 KiB summary: $output_dir/baseline-summary.csv"
