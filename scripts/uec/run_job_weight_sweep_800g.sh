#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/job-weight-sweep-800g}
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
)

run_case() {
  local name=$1
  local ring_weight=$2
  local background_weight=$3
  mkdir -p "$output_dir/$name"
  "$binary" "${common[@]}" --ringJobWeight="$ring_weight" \
    --backgroundJobWeight="$background_weight" \
    --outputPrefix="$output_dir/$name/ecn"
}

run_case startup-budget-only 0 0
run_case equal-1-1 1 1
run_case ring-2-1 2 1
run_case ring-4-1 4 1
run_case ring-8-1 8 1

echo "Job-weight sweep results: $output_dir"
