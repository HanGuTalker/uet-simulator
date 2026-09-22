#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

bash "$script_dir/configure.sh"
bash "$script_dir/build.sh"
bash "$script_dir/test.sh"
