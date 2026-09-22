#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/steady-incast-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
mkdir -p "$output_dir"

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Configure and build the optimized build-perf tree first." >&2
  exit 2
fi

"$binary" \
  --nodes=4 \
  --messages=4 \
  --payloadBytes=67108864 \
  --pattern=incast \
  --fabric=switched \
  --linkRate=800Gbps \
  --linkDelayNs=1000 \
  --queuePackets=10000 \
  --reusePdc=1 \
  --warmupBytes=67108864 \
  --measurementStartUs=5000 \
  --startGapNs=0 \
  --nsccBaseRttNs=4200 \
  --nsccTargetQueueDelayNs=12000 \
  --outputPrefix="$output_dir/steady"

echo "Summary: $output_dir/steady-incast-summary.csv"
