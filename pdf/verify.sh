#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${GOODINPUT_PDF_BUILD_DIR:-${project_root}/build}"

# The fixtures are rebuilt from their script and must not drift from it.
python3 "${project_root}/tools/make-fixtures.py"
git -C "${project_root}" diff --exit-code -- tests/fixtures

cmake -S "${project_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON >/dev/null
cmake --build "${build_dir}" -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ctest --test-dir "${build_dir}" --output-on-failure

if command -v appstreamcli >/dev/null; then
    appstreamcli validate --no-net --explain "${project_root}/data/co.goodinput.PDF.metainfo.xml"
fi
if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "${project_root}/data/co.goodinput.PDF.desktop"
fi

echo "Good Input PDF verification passed."
