#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/baseline-400g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
mkdir -p "$output_dir"

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Configure and build the optimized build-perf tree first." >&2
  exit 2
fi

patterns=(single incast all-to-all)
payload_sizes=(1024 4096 65536 1048576 16777216)
aggregate="$output_dir/baseline-400g-summary.csv"
rm -f "$aggregate"

for payload in "${payload_sizes[@]}"; do
  for pattern in "${patterns[@]}"; do
    prefix="$output_dir/${payload}B"
    "$binary" \
      --nodes=4 \
      --messages=1 \
      --payloadBytes="$payload" \
      --pattern="$pattern" \
      --fabric=switched \
      --linkRate=400Gbps \
      --linkDelayNs=1000 \
      --queuePackets=10000 \
      --outputPrefix="$prefix"
    summary="$prefix-$pattern-summary.csv"
    if [[ ! -f "$aggregate" ]]; then
      head -n 1 "$summary" > "$aggregate"
    fi
    tail -n 1 "$summary" >> "$aggregate"
  done
done

echo "Combined summary: $aggregate"
