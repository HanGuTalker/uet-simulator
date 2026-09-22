#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/background-offset-sweep-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi

common=(
  --nodes=8 --messages=2 --payloadBytes=4194304 --pattern=ring-allreduce
  --ringInterleaved=1 --backgroundAllToAllGroups=1 --backgroundPayloadBytes=1048576
  --fabric=leaf-spine --linkRate=800Gbps --linkDelayNs=1000 --queuePackets=10000
  --reusePdc=1 --warmupBytes=1048576 --measurementStartUs=1500 --startGapNs=0
  --nsccBaseRttNs=6200 --nsccTargetQueueDelayNs=800 --nsccInitialWindowBytes=65536
  --autoScaleInitialWindow=1 --enableEcn=1 --ecnMinBytes=98304
  --ecnMaxBytes=163840 --ecnQueueLimitBytes=524288
  --ringJobWeight=2 --backgroundJobWeight=1 --enableWorkConservingScheduler=1
)

for offset_us in 0 10 20 25 30 50 100; do
  case_dir="$output_dir/offset-${offset_us}us"
  mkdir -p "$case_dir"
  if [[ -f "$case_dir/ecn-ring-allreduce-summary.json" ]]; then
    echo "Skipping completed offset: ${offset_us} us"
    continue
  fi
  "$binary" "${common[@]}" --backgroundStartOffsetNs="$((offset_us * 1000))" \
    --outputPrefix="$case_dir/ecn"
done

echo "Background-offset sweep results: $output_dir"
