#!/usr/bin/env bash
# Run MIC-304 libFuzzer harnesses locally with ASan/UBSan.
#
# Prerequisites:
#   - Clang with libFuzzer (PDF4QT_BUILD_FUZZERS=ON)
#   - Qt + vcpkg deps (see scripts/setup-dev-env.sh)
#
# Usage:
#   ./scripts/fuzz-bughunt.sh [seconds_per_target]
#   ./scripts/fuzz-bughunt.sh 30 fuzz_images   # single target, 30s

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FRISKET_FUZZ_BUILD_DIR:-${REPO_ROOT}/build-fuzz}"
SECONDS_PER_TARGET="${1:-120}"
shift || true

if [[ $# -gt 0 ]]; then
    TARGETS=("$@")
else
    TARGETS=(fuzz_pdf_parser fuzz_stream_filters fuzz_content_stream fuzz_images)
fi

SEED_DIR="${REPO_ROOT}/frisket-preflight/testdata/fixtures"
FUZZ_ARGS=(-max_total_time="${SECONDS_PER_TARGET}" -print_final_stats=1)
if [[ -d "${SEED_DIR}" ]]; then
    FUZZ_ARGS+=("${SEED_DIR}")
fi

export LD_LIBRARY_PATH="${PDF4QT_QT_ROOT:-/opt/Qt/6.9.1/gcc_64}/lib:${LD_LIBRARY_PATH:-}"

for target in "${TARGETS[@]}"; do
    bin="${BUILD_DIR}/usr/bin/${target}"
    if [[ ! -x "${bin}" ]]; then
        echo "Missing fuzz binary: ${bin}" >&2
        echo "Configure with -DPDF4QT_BUILD_FUZZERS=ON -DPDF4QT_ENABLE_SANITIZERS=ON and build the target." >&2
        exit 1
    fi
    echo "======== ${target} (${SECONDS_PER_TARGET}s) ========"
    "${bin}" "${FUZZ_ARGS[@]}"
done
