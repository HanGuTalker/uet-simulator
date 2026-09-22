#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/leaf-spine-validation-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

common=(
  --nodes=8 --fabric=leaf-spine --linkRate=800Gbps --linkDelayNs=1000
  --queuePackets=10000 --reusePdc=1 --warmupBytes=1048576
  --measurementStartUs=1500 --startGapNs=0 --nsccBaseRttNs=6200
  --nsccTargetQueueDelayNs=800 --enableEcn=1 --ecnMinBytes=98304
  --ecnMaxBytes=163840 --ecnQueueLimitBytes=524288
)

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi

run_alltoall() {
  local name=$1
  local bytes=$2
  mkdir -p "$output_dir/all-to-all/$name"
  "$binary" "${common[@]}" --messages=1 --payloadBytes="$bytes" \
    --pattern=all-to-all \
    --outputPrefix="$output_dir/all-to-all/$name/ecn"
}

run_ring() {
  local name=$1
  local interleaved=$2
  mkdir -p "$output_dir/ring-allreduce/$name"
  "$binary" "${common[@]}" --messages=2 --payloadBytes=4194304 \
    --pattern=ring-allreduce --ringInterleaved="$interleaved" \
    --outputPrefix="$output_dir/ring-allreduce/$name/ecn"
}

run_alltoall 1MiB 1048576
run_alltoall 4MiB 4194304
run_alltoall 16MiB 16777216
run_ring contiguous 0
run_ring interleaved 1

echo "Leaf-spine validation results: $output_dir"
