#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/multijob-alltoall-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

common=(
  --nodes=8 --payloadBytes=1048576 --pattern=all-to-all --fabric=leaf-spine
  --linkRate=800Gbps --linkDelayNs=1000 --queuePackets=10000 --reusePdc=0
  --warmupBytes=0 --measurementStartUs=100 --nsccBaseRttNs=6200
  --nsccTargetQueueDelayNs=800 --enableEcn=1 --ecnMinBytes=98304
  --ecnMaxBytes=163840 --ecnQueueLimitBytes=524288
)

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi

run_case() {
  local name=$1
  local jobs=$2
  local gap_ns=$3
  local initial_window=$4
  local auto_scale=${5:-0}
  mkdir -p "$output_dir/$name"
  "$binary" "${common[@]}" --messages="$jobs" --startGapNs="$gap_ns" \
    --nsccInitialWindowBytes="$initial_window" \
    --autoScaleInitialWindow="$auto_scale" \
    --outputPrefix="$output_dir/$name/ecn"
}

run_case 1job-default 1 0 65536
run_case 2jobs-default 2 0 65536
run_case 4jobs-default 4 0 65536
run_case 4jobs-staggered 4 5000 65536
run_case 4jobs-iw16KiB 4 0 16384
run_case 2jobs-auto 2 0 65536 1
run_case 4jobs-auto 4 0 65536 1

echo "Multi-job All-to-All results: $output_dir"
