#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/fat-tree-validation-800g}
binary=${AI_TRANSPORT_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Build it with: ./ns3 build uet-ai-workload-example" >&2
  exit 2
fi

mkdir -p "$output_dir"
common=(
  --transport=uec
  --operation=message
  --nodes=16
  --messages=1
  --payloadBytes=1024
  --fabric=fat-tree
  --fatTreeK=4
  --linkRate=800Gbps
  --hostLinkDelayNs=100
  --fabricLinkDelayNs=250
  --switchProcessingDelayNs=250
  --nsccBaseRttNs=0
  --nsccInitialWindowBytes=765000
)

echo "Running one cross-pod UEC message on the k=4 fat-tree"
"$binary" "${common[@]}" \
  --pattern=single \
  --outputPrefix="$output_dir/single"

echo "Running 16-node UEC all-to-all on the k=4 fat-tree"
"$binary" "${common[@]}" \
  --pattern=all-to-all \
  --startGapNs=1000 \
  --outputPrefix="$output_dir/alltoall"

python3 scripts/transport/validate_fat_tree.py "$output_dir"
