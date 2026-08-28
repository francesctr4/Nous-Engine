#!/usr/bin/env bash
# Summarise line coverage of Source/Engine from a NOUS_COVERAGE build.
#
#   sudo dnf install lcov            # Fedora  (Debian/Ubuntu: apt install lcov)
#   cmake --preset Coverage-Linux
#   cmake --build Build/Coverage-Linux
#   ctest --test-dir Build/Coverage-Linux --output-on-failure
#   Tools/Coverage/report.sh Build/Coverage-Linux
#
# Prints a per-file table and one overall percentage, and writes an HTML report
# when genhtml is available.
set -euo pipefail

BUILD_DIR="${1:-Build/Coverage-Linux}"
OUT_DIR="${BUILD_DIR}/coverage"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "no such build dir: ${BUILD_DIR}" >&2
    exit 1
fi
command -v lcov >/dev/null || {
    echo "lcov not installed (Fedora: sudo dnf install lcov)" >&2
    exit 1
}

mkdir -p "${OUT_DIR}"

# lcov 2.x spells this branch_coverage=1, lcov 1.x spells it
# lcov_branch_coverage=1, and passing the wrong one is a hard error. Probe rather
# than assume whatever the distro ships.
BRANCH_RC=()
if lcov --rc branch_coverage=1 --version >/dev/null 2>&1; then
    BRANCH_RC=(--rc branch_coverage=1)
elif lcov --rc lcov_branch_coverage=1 --version >/dev/null 2>&1; then
    BRANCH_RC=(--rc lcov_branch_coverage=1)
fi

# gcov must match the compiler that produced the .gcno files. Under clang the
# system gcov cannot read them, so route lcov through llvm-cov's gcov mode.
GCOV_TOOL=()
if [[ "${CXX:-}" == *clang* ]] && command -v llvm-cov >/dev/null; then
    {
        echo '#!/bin/sh'
        echo 'exec llvm-cov gcov "$@"'
    } > "${OUT_DIR}/llvm-gcov.sh"
    chmod +x "${OUT_DIR}/llvm-gcov.sh"
    GCOV_TOOL=(--gcov-tool "${OUT_DIR}/llvm-gcov.sh")
fi

# --ignore-errors keeps one unreadable .gcda from aborting the whole run: a test
# that crashed mid-write leaves exactly that behind.
lcov --capture \
     --directory "${BUILD_DIR}" \
     --output-file "${OUT_DIR}/raw.info" \
     "${BRANCH_RC[@]}" "${GCOV_TOOL[@]}" \
     --ignore-errors mismatch,gcov,source,empty \
     >/dev/null

# Keep only this engine's own sources. Everything else -- vcpkg headers, the STL,
# gtest -- would distort the number, and the tests would flatter it by counting
# themselves.
lcov --extract "${OUT_DIR}/raw.info" "${REPO_ROOT}/Source/Engine/*" \
     --output-file "${OUT_DIR}/engine.info" \
     --ignore-errors empty,unused \
     >/dev/null

lcov --remove "${OUT_DIR}/engine.info" "*/test/*" "*/ThirdParty/*" \
     --output-file "${OUT_DIR}/engine.info" \
     --ignore-errors empty,unused \
     >/dev/null

echo
echo "=== Nous Engine line coverage (Source/Engine, tests excluded) ==="
lcov --list "${OUT_DIR}/engine.info" --ignore-errors empty

if command -v genhtml >/dev/null; then
    genhtml "${OUT_DIR}/engine.info" \
            --output-directory "${OUT_DIR}/html" \
            --ignore-errors source,empty \
            >/dev/null
    echo
    echo "HTML report: ${OUT_DIR}/html/index.html"
fi
