#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/leaf-spine-ecn-sweep-800g}
binary=${UET_BENCH_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  exit 2
fi

common=(
  --nodes=8
  --messages=1
  --payloadBytes=4194304
  --pattern=all-to-all
  --fabric=leaf-spine
  --linkRate=800Gbps
  --linkDelayNs=1000
  --reusePdc=1
  --warmupBytes=1048576
  --measurementStartUs=1500
  --startGapNs=0
  --nsccBaseRttNs=6200
  --nsccTargetQueueDelayNs=800
)

run_ecn() {
  local name=$1
  local min_bytes=$2
  local max_bytes=$3
  mkdir -p "$output_dir/$name"
  "$binary" "${common[@]}" \
    --queuePackets=10000 \
    --enableEcn=1 \
    --ecnMinBytes="$min_bytes" \
    --ecnMaxBytes="$max_bytes" \
    --ecnQueueLimitBytes=524288 \
    --outputPrefix="$output_dir/$name/ecn"
}

mkdir -p "$output_dir/droptail"
"$binary" "${common[@]}" \
  --queuePackets=125 \
  --enableEcn=0 \
  --outputPrefix="$output_dir/droptail/baseline"

run_ecn ecn-64-96KiB 65536 98304
run_ecn ecn-96-160KiB 98304 163840
run_ecn ecn-128-256KiB 131072 262144

echo "Leaf-spine ECN sweep results: $output_dir"
