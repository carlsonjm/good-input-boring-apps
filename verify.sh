#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${OFFICE_BUILD_DIR:-${project_root}/build}"

cmake -S "${project_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON >/dev/null
cmake --build "${build_dir}" -j"$(nproc)"
ctest --test-dir "${build_dir}" --output-on-failure

echo "Good Input Office verification passed."
