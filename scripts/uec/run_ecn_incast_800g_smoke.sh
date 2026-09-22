#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/ecn-incast-800g-smoke}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
mkdir -p "$output_dir"

"$binary" \
  --nodes=4 \
  --messages=2 \
  --payloadBytes=1048576 \
  --pattern=incast \
  --fabric=switched \
  --linkRate=800Gbps \
  --linkDelayNs=1000 \
  --queuePackets=10000 \
  --reusePdc=1 \
  --warmupBytes=1048576 \
  --measurementStartUs=500 \
  --startGapNs=0 \
  --nsccBaseRttNs=4200 \
  --nsccTargetQueueDelayNs=800 \
  --enableEcn=1 \
  --ecnMinBytes=65536 \
  --ecnMaxBytes=98304 \
  --ecnQueueLimitBytes=524288 \
  --outputPrefix="$output_dir/closed-loop"

