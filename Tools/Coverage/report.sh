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
#
# MAINTENANCE NOTE: keep every comment OUTSIDE the backslash-continued commands
# below. A '#' on a continued line is joined into the command before tokenisation
# and silently discards the rest of the arguments -- that is exactly how this
# script once ended up invoking geninfo with no --ignore-errors at all, turning a
# harmless libstdc++ debug-info warning into a fatal error.
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

# What each ignored class is for:
#   mismatch  GCC 16 debug-info quirks in libstdc++ headers. lcov 2.x makes these
#             fatal, and they say nothing about this engine's code.
#   gcov      one unreadable .gcda (a test that crashed mid-write) must not abort
#             the whole run.
#   source    generated or third-party sources lcov cannot locate.
#   empty     targets that produced no data at all.
#   unused    an --extract/--remove pattern that matched nothing.
#   negative  safety net only. The real fix is -fprofile-update=atomic in
#             Coverage.cmake; if this fires, a threaded test wrote a counter
#             non-atomically and that file's numbers are understated.
IGNORE=mismatch,gcov,source,empty,unused,negative

lcov --capture \
     --directory "${BUILD_DIR}" \
     --output-file "${OUT_DIR}/raw.info" \
     "${BRANCH_RC[@]}" "${GCOV_TOOL[@]}" \
     --ignore-errors "${IGNORE}" \
     >/dev/null

# Keep only this engine's own sources. Everything else -- vcpkg headers, the STL,
# gtest -- would distort the number, and the tests would flatter it by counting
# themselves.
lcov --extract "${OUT_DIR}/raw.info" "${REPO_ROOT}/Source/Engine/*" \
     --output-file "${OUT_DIR}/engine.info" \
     --ignore-errors "${IGNORE}" \
     >/dev/null

lcov --remove "${OUT_DIR}/engine.info" "*/test/*" "*/ThirdParty/*" \
     --output-file "${OUT_DIR}/engine.info" \
     --ignore-errors "${IGNORE}" \
     >/dev/null

echo
echo "=== Nous Engine line coverage (Source/Engine, tests excluded) ==="
lcov --list "${OUT_DIR}/engine.info" --ignore-errors "${IGNORE}"

if command -v genhtml >/dev/null; then
    genhtml "${OUT_DIR}/engine.info" \
            --output-directory "${OUT_DIR}/html" \
            --ignore-errors "${IGNORE}" \
            >/dev/null
    echo
    echo "HTML report: ${OUT_DIR}/html/index.html"
fi
