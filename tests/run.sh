#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${AERO7_BUILD_DIR:-$project_root/build}"

"$project_root/tests/ci/validate-static.sh"
cmake -S "$project_root" -B "$build_root" -G Ninja -DBUILD_TESTING=ON
cmake --build "$build_root"
ctest --test-dir "$build_root" --output-on-failure
