#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

output_dir=${1:-results/ecmp-comparison-800g}
binary=${AI_TRANSPORT_BINARY:-build-perf/src/uet/examples/ns3.47-uet-ai-workload-example-optimized}
nodes=${NODES:-4}
spines=${SPINES:-4}
messages=${MESSAGES:-1}
payload_bytes=${PAYLOAD_BYTES:-65536}
patterns=${PATTERNS:-all-to-all}
protocols=${PROTOCOLS:-"uec rocev2 veroce mrc falcon"}

if [[ ! -x "$binary" ]]; then
  echo "Benchmark binary not found: $binary" >&2
  echo "Build it with: ./ns3 build uet-ai-workload-example" >&2
  exit 2
fi

mkdir -p "$output_dir"
combined="$output_dir/comparison-summary.csv"
first_summary=1

for protocol in $protocols; do
  for pattern in $patterns; do
    prefix="$output_dir/$protocol"
    echo "Running $protocol/$pattern on ${nodes}-node, ${spines}-spine 800 Gbps fabric"
    "$binary" \
      --transport="$protocol" \
      --operation=message \
      --pattern="$pattern" \
      --nodes="$nodes" \
      --spines="$spines" \
      --messages="$messages" \
      --payloadBytes="$payload_bytes" \
      --fabric=leaf-spine \
      --linkRate=800Gbps \
      --hostLinkDelayNs=100 \
      --fabricLinkDelayNs=250 \
      --switchProcessingDelayNs=250 \
      --nsccBaseRttNs=0 \
      --outputPrefix="$prefix"

    summary="$prefix-$pattern-summary.csv"
    if (( first_summary )); then
      cp "$summary" "$combined"
      first_summary=0
    else
      tail -n +2 "$summary" >> "$combined"
    fi
  done
done

echo "Combined comparison summary: $combined"
