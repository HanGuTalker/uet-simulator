#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/mixed-collectives-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi

common=(
  --nodes=8 --messages=2 --payloadBytes=4194304 --pattern=ring-allreduce
  --backgroundAllToAllGroups=1 --backgroundPayloadBytes=1048576
  --fabric=leaf-spine --linkRate=800Gbps --linkDelayNs=1000 --queuePackets=10000
  --reusePdc=1 --warmupBytes=1048576 --measurementStartUs=1500 --startGapNs=0
  --nsccBaseRttNs=6200 --nsccTargetQueueDelayNs=800
  --enableEcn=1 --ecnMinBytes=98304
  --ecnMaxBytes=163840 --ecnQueueLimitBytes=524288
)

run_case() {
  local name=$1
  local interleaved=$2
  local auto_scale=$3
  local initial_window=$4
  mkdir -p "$output_dir/$name"
  "$binary" "${common[@]}" --ringInterleaved="$interleaved" \
    --autoScaleInitialWindow="$auto_scale" --nsccInitialWindowBytes="$initial_window" \
    --outputPrefix="$output_dir/$name/ecn"
}

run_case contiguous-group-fair 0 1 65536
run_case interleaved-equal-pdc 1 0 32768
run_case interleaved-group-fair 1 1 65536

echo "Mixed collective results: $output_dir"
