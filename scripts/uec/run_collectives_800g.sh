#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/collectives-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Configure and build the optimized build-perf tree first." >&2
  exit 2
fi

run_case() {
  local pattern=$1
  local size_name=$2
  local payload_bytes=$3
  local case_dir="$output_dir/$pattern/$size_name"
  mkdir -p "$case_dir"
  "$binary" \
    --nodes=4 \
    --messages=2 \
    --payloadBytes="$payload_bytes" \
    --pattern="$pattern" \
    --fabric=switched \
    --linkRate=800Gbps \
    --linkDelayNs=1000 \
    --queuePackets=10000 \
    --reusePdc=1 \
    --warmupBytes=1048576 \
    --measurementStartUs=1000 \
    --startGapNs=0 \
    --nsccBaseRttNs=4200 \
    --nsccTargetQueueDelayNs=800 \
    --enableEcn=1 \
    --ecnMinBytes=65536 \
    --ecnMaxBytes=98304 \
    --ecnQueueLimitBytes=524288 \
    --outputPrefix="$case_dir/ecn"
}

run_case all-to-all 1MiB 1048576
run_case all-to-all 4MiB 4194304
run_case ring-allreduce 1MiB 1048576
run_case ring-allreduce 4MiB 4194304

echo "Collective results: $output_dir"
