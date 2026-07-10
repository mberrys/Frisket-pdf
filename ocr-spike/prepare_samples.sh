#!/usr/bin/env bash
# MIT License — see repo root. MIC-128: rasterize PDFs with PdfTool when built.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PDFTOOL="${PDFTOOL:-}"
if [[ -z "${PDFTOOL}" ]]; then
  for candidate in \
    "${ROOT}/../build/usr/bin/PdfTool" \
    "${ROOT}/../build/PdfTool/PdfTool" \
    "$(command -v PdfTool 2>/dev/null || true)"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      PDFTOOL="${candidate}"
      break
    fi
  done
fi

if [[ -z "${PDFTOOL}" || ! -x "${PDFTOOL}" ]]; then
  echo "PdfTool not found. Build PdfTool or set PDFTOOL=/path/to/PdfTool" >&2
  echo "Falling back to Python fixtures: python3 generate_fixtures.py" >&2
  exec python3 "${ROOT}/generate_fixtures.py"
fi

OUT_RASTER="${ROOT}/fixtures/rasters"
OUT_TEXT="${ROOT}/fixtures/fetch-text"
mkdir -p "${OUT_RASTER}" "${OUT_TEXT}"

# Generate example PDFs if missing
EXAMPLE_DIR="${ROOT}/fixtures/pdfs"
mkdir -p "${EXAMPLE_DIR}"
if [[ ! -f "${EXAMPLE_DIR}/menu-live-text.pdf" ]]; then
  python3 "${ROOT}/generate_fixtures.py"
fi

shopt -s nullglob
PDFS=("${EXAMPLE_DIR}"/*.pdf)
if [[ ${#PDFS[@]} -eq 0 ]]; then
  echo "No PDFs in ${EXAMPLE_DIR}. Add job PDFs or run PdfExampleGenerator in repo root." >&2
  exit 1
fi

for pdf in "${PDFS[@]}"; do
  base="$(basename "${pdf}" .pdf)"
  echo "==> render ${pdf}"
  "${PDFTOOL}" render "${pdf}" \
    --image-res-mode dpi \
    --image-res-dpi 300 \
    --image-template-fn "${OUT_RASTER}/${base}-p%03d.png" \
    --page-first 1 --page-last 1

  echo "==> fetch-text ${pdf}"
  "${PDFTOOL}" fetch-text "${pdf}" \
    --page-first 1 --page-last 1 \
    --console-format text > "${OUT_TEXT}/${base}-p001.txt" || true
done

echo "Done. Update fixtures/manifest.json ground_truth from fetch-text output if using real PDFs."
