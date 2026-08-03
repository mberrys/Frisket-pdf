#!/usr/bin/env bash
# Validates a Frisket-PDF AppImage on a clean machine (MIC-301 Linux half).
#
# Usage:
#   scripts/smoke-test-appimage.sh /path/to/Frisket-pdf-VERSION-x86_64.AppImage [test.pdf]
#
# Asserts the shipped layout resolves, runs PdfTool preflight against a fixture,
# and scans the tree for payloads the default V1 bundle must not ship (MIC-343).

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <AppImage> [test.pdf]" >&2
    exit 1
fi

APPIMAGE_PATH="$(readlink -f "$1")"
if [[ ! -f "$APPIMAGE_PATH" ]]; then
    echo "AppImage not found: $APPIMAGE_PATH" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TEST_PDF="${2:-}"
if [[ -z "$TEST_PDF" ]]; then
    TEST_PDF="${REPO_ROOT}/frisket-preflight/testdata/fixtures/bleed-adequate.pdf"
fi
if [[ ! -f "$TEST_PDF" ]]; then
    echo "Test PDF not found: $TEST_PDF" >&2
    exit 1
fi

EXTRACT_ROOT="$(mktemp -d)"
cleanup() {
    rm -rf "$EXTRACT_ROOT"
}
trap cleanup EXIT

chmod +x "$APPIMAGE_PATH"
(
    cd "$EXTRACT_ROOT"
    "$APPIMAGE_PATH" --appimage-extract >/dev/null
)

ROOT="${EXTRACT_ROOT}/squashfs-root"
if [[ ! -d "$ROOT" ]]; then
    echo "AppImage extract failed: missing squashfs-root under $EXTRACT_ROOT" >&2
    exit 1
fi

BIN_DIR="${ROOT}/usr/bin"
LIB_DIR="${ROOT}/usr/lib"
PLUGINS_DIR="${ROOT}/usr/lib/pdf4qt"
PROFILES_DIR="${ROOT}/usr/share/frisket/profiles"

assert_file() {
    local path="$1"
    local label="$2"
    if [[ ! -e "$path" ]]; then
        echo "Missing ${label}: ${path}" >&2
        exit 1
    fi
    echo "OK: ${label}"
}

echo "Smoke-testing AppImage at ${APPIMAGE_PATH}"

assert_file "${BIN_DIR}/Pdf4QtEditor" "Editor"
assert_file "${BIN_DIR}/PdfTool" "PdfTool"
assert_file "${PLUGINS_DIR}/libFrisketPreflightPlugin.so" "Frisket preflight plugin"
assert_file "${PROFILES_DIR}/frisket-default.json" "Default preflight profile"
assert_file "${PROFILES_DIR}/schemas/profile.schema.json" "Profile schema"
assert_file "${PROFILES_DIR}/schemas/report.schema.json" "Report schema"

OCR_PLUGIN="${PLUGINS_DIR}/libOcrPlugin.so"
if [[ -e "$OCR_PLUGIN" ]]; then
    echo "OcrPlugin found at ${OCR_PLUGIN}. V1 ships OCR as CLI-only (MIC-343)." >&2
    exit 1
fi
echo "OK: OcrPlugin absent (V1 CLI-only OCR surface, MIC-343)"

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export LD_LIBRARY_PATH="${LIB_DIR}:${LIB_DIR}/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

PDF_TOOL="${BIN_DIR}/PdfTool"
PROFILE_PATH="${PROFILES_DIR}/frisket-default.json"

set +e
PREFLIGHT_OUTPUT="$("$PDF_TOOL" preflight "$TEST_PDF" --profile "$PROFILE_PATH" --console-format text 2>&1)"
PREFLIGHT_EXIT=$?
set -e

if [[ "$PREFLIGHT_EXIT" -ne 0 && "$PREFLIGHT_EXIT" -ne 1 ]]; then
    echo "PdfTool preflight failed with unexpected exit code ${PREFLIGHT_EXIT}:" >&2
    echo "$PREFLIGHT_OUTPUT" >&2
    exit 1
fi
echo "OK: PdfTool preflight completed (exit ${PREFLIGHT_EXIT})"

set +e
HELP_OUTPUT="$("$PDF_TOOL" help 2>&1)"
HELP_EXIT=$?
set -e
if [[ "$HELP_EXIT" -ne 0 ]]; then
    echo "PdfTool help failed with exit code ${HELP_EXIT}:" >&2
    echo "$HELP_OUTPUT" >&2
    exit 1
fi
echo "OK: PdfTool help"

# docs/PACKAGING_LICENSING.md: default bundle is C++/Qt only.
FORBIDDEN_HITS=()
while IFS= read -r hit; do
    FORBIDDEN_HITS+=("$hit")
done < <(find "$ROOT" -type f \( \
    -name 'python*' -o -name 'java' -o -name 'javaw' -o -name 'gs' -o -name 'gswin*.exe' \
    \) 2>/dev/null | head -n 20)

if [[ "${#FORBIDDEN_HITS[@]}" -gt 0 ]]; then
    echo "Forbidden payload found in the AppImage tree (docs/PACKAGING_LICENSING.md):" >&2
    printf '  %s\n' "${FORBIDDEN_HITS[@]}" >&2
    exit 1
fi
echo "OK: no Ghostscript / JRE / Python payload in the default bundle"

echo "AppImage smoke test passed."
