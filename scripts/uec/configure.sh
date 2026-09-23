#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

python3 ./ns3 configure \
  --build-profile=debug \
  --enable-tests \
  --enable-examples \
  --enable-modules=uet,ai-transport \
  -- -G Ninja
