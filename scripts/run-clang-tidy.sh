#!/usr/bin/env bash
# Run clang-tidy against Frisket-PDF sources using compile_commands.json.
#
# Prerequisites:
#   - clang-tidy-18 (or set CLANG_TIDY)
#   - Configured build dir with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
#     (see scripts/setup-dev-env.sh or cmake configure in AGENTS.md)
#
# Usage:
#   ./scripts/run-clang-tidy.sh [path ...]        # default: Pdf4QtLibCore PdfTool
#   ./scripts/run-clang-tidy.sh --fix [path ...]
#   ./scripts/run-clang-tidy.sh --all             # all project .cpp under repo
#
# Environment:
#   FRISKET_BUILD_DIR   build directory (default: ./build)
#   CLANG_TIDY          clang-tidy binary (default: clang-tidy-18)
#   CLANG_TIDY_JOBS     parallel jobs (default: nproc)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FRISKET_BUILD_DIR:-${REPO_ROOT}/build}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy-18}"
JOBS="${CLANG_TIDY_JOBS:-$(nproc)}"
FIX=0
SCAN_ALL=0
PATHS=()

log() { printf '>>> %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix) FIX=1; shift ;;
        --all) SCAN_ALL=1; shift ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        --) shift; PATHS+=("$@"); break ;;
        -*) die "Unknown option: $1" ;;
        *) PATHS+=("$1"); shift ;;
    esac
done

if ! command -v "${CLANG_TIDY}" >/dev/null 2>&1; then
    die "clang-tidy not found (${CLANG_TIDY}). Install clang-tidy-18 or set CLANG_TIDY."
fi

COMPILE_DB="${BUILD_DIR}/compile_commands.json"
if [[ ! -f "${COMPILE_DB}" ]]; then
    die "Missing ${COMPILE_DB}. Configure the build with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON."
fi

# Prefer run-clang-tidy when available (parallel, respects compile DB).
RUN_CLANG_TIDY="${CLANG_TIDY/-tidy/-tools/run-clang-tidy}"
if [[ ! -x "${RUN_CLANG_TIDY}" && -x "/usr/bin/run-clang-tidy-18" ]]; then
    RUN_CLANG_TIDY="/usr/bin/run-clang-tidy-18"
fi

collect_sources() {
    local root="$1"
  if [[ -d "${root}" ]]; then
        find "${root}" \( -name '*.cpp' -o -name '*.cxx' -o -name '*.cc' \) \
            ! -path '*/build/*' \
            ! -path '*/.git/*' \
            ! -name 'moc_*.cpp' \
            ! -name 'qrc_*.cpp' \
            ! -name 'ui_*.cpp' \
            -print
    elif [[ -f "${root}" ]]; then
        printf '%s\n' "${root}"
    fi
}

if [[ ${SCAN_ALL} -eq 1 ]]; then
    mapfile -t SOURCES < <(find "${REPO_ROOT}" \( -name '*.cpp' -o -name '*.cxx' -o -name '*.cc' \) \
        ! -path '*/build/*' \
        ! -path '*/.git/*' \
        ! -path '*/vcpkg/*' \
        ! -name 'moc_*.cpp' \
        ! -name 'qrc_*.cpp' \
        ! -name 'ui_*.cpp' \
        -print | sort)
elif [[ ${#PATHS[@]} -eq 0 ]]; then
    mapfile -t SOURCES < <(
        collect_sources "${REPO_ROOT}/Pdf4QtLibCore"
        collect_sources "${REPO_ROOT}/PdfTool"
    )
else
    mapfile -t SOURCES < <(
        for p in "${PATHS[@]}"; do
            if [[ "${p}" != /* ]]; then
                p="${REPO_ROOT}/${p}"
            fi
            collect_sources "${p}"
        done
    )
fi

if [[ ${#SOURCES[@]} -eq 0 ]]; then
    die "No source files matched."
fi

log "clang-tidy ${CLANG_TIDY} on ${#SOURCES[@]} file(s); compile DB: ${COMPILE_DB}"

EXTRA_ARGS=()
if [[ ${FIX} -eq 1 ]]; then
    EXTRA_ARGS+=(-fix)
fi

# compile_commands.json is generated for GCC; clang-tidy needs libstdc++ include paths.
if grep -q '/usr/bin/g++' "${COMPILE_DB}" 2>/dev/null; then
    while IFS= read -r inc; do
        case "${inc}" in
            */gcc/*/include) continue ;;  # clang chokes on GCC intrinsic headers
        esac
        EXTRA_ARGS+=(-extra-arg=-isystem"${inc}")
    done < <(g++ -E -x c++ - -v < /dev/null 2>&1 | sed -n '/#include <...>/,/^End/p' | grep '^ ' | sed 's/^ //')
fi

if [[ -x "${RUN_CLANG_TIDY}" ]]; then
    log "Using ${RUN_CLANG_TIDY} (-j ${JOBS})"
    "${RUN_CLANG_TIDY}" -p "${BUILD_DIR}" -j "${JOBS}" \
        -clang-tidy-binary "$(command -v "${CLANG_TIDY}")" \
        "${EXTRA_ARGS[@]}" \
        "${SOURCES[@]}"
else
    log "run-clang-tidy not found; running ${CLANG_TIDY} sequentially"
    status=0
    for src in "${SOURCES[@]}"; do
        if ! "${CLANG_TIDY}" -p "${BUILD_DIR}" "${EXTRA_ARGS[@]}" "${src}"; then
            status=1
        fi
    done
    exit "${status}"
fi
