#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

python3 ./ns3 build ai-transport
python3 ./ns3 build roce
python3 ./ns3 build veroce
python3 ./ns3 build uet
