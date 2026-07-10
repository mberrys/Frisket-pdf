# MIC-128 findings: EasyOCR vs Unlimited-OCR

**Date:** 2026-07-10  
**Linear:** [MIC-128](https://linear.app/mbx2/issue/MIC-128/spike-test-easyocr-locally-as-an-alternative-to-unlimited-ocr)  
**Environment:** Cloud agent VM, CPU-only (no CUDA), Python 3.12, EasyOCR 1.7.x + PyTorch CPU.

## Executive summary

EasyOCR is a **viable functional fallback** for MIC-123 (text + axis-aligned boxes + per-line confidence) but is **too slow on CPU** for interactive Editor use at print resolution (~11 s/page at 300 DPI letter). Recommend proceeding with **Unlimited-OCR as the primary MIC-123 engine** (GPU-oriented Transformers stack) while keeping EasyOCR documented as a **local/dev CPU path** for spike validation and offline batch on small page counts.

There is **no onboard OCR** in Frisket-PDF to replace — the decision is which **external** engine backs the sidecar service.

## Test methodology

| Step | Tool | Notes |
|------|------|-------|
| Rasterize pages | `PdfTool render` (preferred) or `generate_fixtures.py` | 300 DPI, ~2550×3300 px letter |
| Live-text baseline | `PdfTool fetch-text` (preferred) or manifest `ground_truth` | Compared via RapidFuzz token sort ratio |
| OCR | `run_easyocr_spike.py` | `easyocr.Reader(['en'], gpu=False)` |
| Output | `results/spike_report.json` | MIC-123 field parity checked |

**Fixture caveat:** This run used **synthetic rasters** (`generate_fixtures.py`) because PdfTool was not built on the spike VM. `prepare_samples.sh` is ready to re-run against real job PDFs when `PDFTOOL` is set. Re-validate accuracy on client menus/scans before production lock-in.

### Fixtures

| ID | Kind | Description |
|----|------|-------------|
| `menu-live-text` | live-text | Venue menu headings + prices |
| `spec-sheet` | live-text | Dense prepress spec + body paragraph |
| `scan-like-menu` | scan-like | Menu raster with JPEG artifacts + 1.2° skew + blur |

## Measured results (EasyOCR, CPU)

| Fixture | ms/page | Detections | Mean conf | vs ground truth (token sort) |
|---------|---------|------------|-----------|------------------------------|
| menu-live-text | 10,398 | 11 | 0.85 | **100.0%** |
| spec-sheet | 11,410 | 12 | 0.78 | **99.0%** |
| scan-like-menu | 10,887 | 11 | 0.86 | **98.0%** |

**Aggregate:** mean **10,898 ms/page** (min 10,398 / max 11,410). Reader cold-start **1,454 ms** (models cached after first run).

### Accuracy notes

- Clean vector-derived rasters: excellent recall; prices split into separate detections (good for Quote Creation Feed boxes later).
- Spec sheet: minor character substitutions (`=` for `—`, `PDFIX-la` for `PDF/X-1a`) — still ~99% fuzzy match.
- Scan-like: `$6.00` → `s6.00` (expected degradation on compressed/skewed input).

### MIC-123 schema parity

| Field | EasyOCR | Notes |
|-------|---------|-------|
| `text` | Yes | Joined detection strings |
| `boxes` | Yes | Quadrilateral → axis-aligned `[x0,y0,x1,y1]` in pixel space |
| `confidence` | Yes | Per detection, 0–1 |
| `page` | Yes | Passed through from manifest |

**Parity: full** for planned HTTP contract. EasyOCR does not return reading order guarantees; sort by `(y, x)` may be needed for multi-column menus.

## Comparison vs Unlimited-OCR (planned MIC-123)

| Criterion | EasyOCR (this spike) | Unlimited-OCR (planned) |
|-----------|----------------------|-------------------------|
| **Accuracy (clean print)** | 98–100% on synthetic fixtures | TBD — run same corpus when MIC-123 service stands up |
| **Accuracy (degraded scan)** | Good; occasional glyph swaps on `$` | TBD — likely stronger on hard scans (doc-level model) |
| **Latency CPU** | ~11 s/page @ 300 DPI letter | Expected slower without GPU; not evaluated here |
| **Latency GPU** | Not tested | Primary design target for MIC-123 |
| **Setup pain** | `pip install easyocr` (~600 MB torch+models); first run downloads weights | Hugging Face Transformers + model checkout; heavier deps; needs GPU for practical throughput |
| **Service packaging** | Trivial `python run_easyocr_spike.py`; wrap in FastAPI later | Same FastAPI shell; swap engine backend |
| **Boxes + confidence** | Native | Expected native (verify in MIC-123 spike) |
| **License** | Apache 2.0 (EasyOCR) | Check Unlimited-OCR / model card before ship |

## Setup pain (EasyOCR)

**Pros**

- Single `pip install -r requirements.txt` in an isolated venv
- No C++/vcpkg/Qt coupling
- English model auto-downloads on first `Reader()` init (~1.5 s after cache)

**Cons**

- Pulls **PyTorch CPU** wheel (~large disk footprint)
- **~11 s/page** at 300 DPI on 4-core cloud VM — unacceptable for in-Editor synchronous OCR
- GPU path not validated in this spike (`--gpu` flag exists in script)

## Recommendations

1. **MIC-123:** Implement the FastAPI sidecar with **Unlimited-OCR first**; keep response schema identical to `run_easyocr_spike.py` output.
2. **MIC-128 follow-up:** Re-run `prepare_samples.sh` on 3–5 real shop PDFs (menu, spec sheet, phone scan) once PdfTool is built; append rows to `spike_report.json`.
3. **MIC-124 gating:** Do not call either engine when `fetch-text` returns usable live text — OCR cost dominates at ~11 s/page CPU.
4. **Resolution policy:** Consider **150 DPI** raster for OCR gating pass (halve pixels ≈ quarter area) if accuracy holds; benchmark in MIC-123.
5. **EasyOCR retention:** Keep `ocr-spike/` as a **regression harness** for engine swaps; do not add to vcpkg.

## Artifacts

- `results/spike_report.json` — full per-page detections and timings
- `fixtures/manifest.json` — ground truth for synthetic fixtures
- Scripts: `generate_fixtures.py`, `run_easyocr_spike.py`, `prepare_samples.sh`

## Decision for OCR Module milestone

| Question | Answer |
|----------|--------|
| Does EasyOCR replace onboard OCR? | N/A — no onboard OCR exists |
| Is EasyOCR good enough for Frisket? | **Functionally yes** (schema + accuracy); **operationally no** on CPU for Editor |
| Primary engine for MIC-123? | **Unlimited-OCR** (pending GPU latency confirmation) |
| Keep EasyOCR? | **Yes** as dev/CI fallback and comparison baseline |
