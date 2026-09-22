#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_dir"

python3 ./test.py -s uet --verbose
python3 ./test.py --no-build --fullness=QUICK --jobs="${NS3_TEST_JOBS:-2}" --multiple \
  --verbose-failed
