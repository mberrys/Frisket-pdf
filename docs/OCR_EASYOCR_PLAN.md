# EasyOCR V1 — Design Plan (M0)

Status: **locked for V1 implementation** (2026-07-22).
Scope: Frisket-pdf / PDF4QT 1.6.0.0. Read-only OCR intake (JSON report + Editor panel).
Engine: **EasyOCR (CPU)** in bundled `FrisketOcrService` sidecar under `frisket-ocr/`.
Epic: [MIC-123](https://linear.app/mbx2/issue/MIC-123). Page gate: [MIC-124](https://linear.app/mbx2/issue/MIC-124).

## Goal

Add **read-only OCR** for scanned or image-only PDF pages: classify pages, rasterize candidates, run EasyOCR via a stdio JSON sidecar, emit a normalized JSON report. No PDF rewrite, no preflight pass/fail changes in V1.

## Non-goals

- Searchable-PDF write-back
- Quote extraction (MIC-127)
- Preflight findings based on OCR text (MIC-125 follow-up)
- PageMaster batch OCR
- tiny-ocr / Unlimited-OCR training pipeline (canceled)

## Architecture

| Layer | Role |
|-------|------|
| **Core** `PDFOcrPageGate` | Per-page gate: skip live text, OCR image-only pages |
| **Core** `PDFOcrReport` | Report struct + `toJson()` matching schema |
| **PdfTool** `ocr` | Gate → render @ DPI → long-lived sidecar → stdout JSON |
| **Editor** `OcrPlugin` | `QProcess` → `PdfTool ocr`; dock widget for results |
| **Sidecar** `FrisketOcrService` | EasyOCR stdio JSON-line service (PyInstaller bundle) |

```text
PdfTool ocr scan.pdf --console-format json
    -> PDFOcrPageGate per page
    -> render PNG for NeedsOcr pages
    -> FrisketOcrService (stdin/stdout JSON lines)
    -> PDFOcrReport -> stdout
```

## V1 locks

| Item | Value |
|------|-------|
| Engine | EasyOCR CPU, default languages `["en"]` |
| Render DPI | 300 |
| Gate threshold | ≥ 20 non-whitespace chars from `PDFDocumentTextFlow` → skip |
| Image-only heuristic | No live text + page has image XObject (graphic piece `Image`) → OCR |
| Sidecar IPC | stdio JSON lines: one request per page image path, one response line |
| Schema | `frisket-ocr/schemas/ocr-report.schema.json` v1 |
| Install layout | `<bindir>/FrisketOcrService/FrisketOcrService.exe`; models under `%ProgramData%/Frisket/ocr-models` on first run |
| Exit codes | 0 success; 1 partial page failures; 2 usage/contract; 3 sidecar unavailable |

## Report contract

Top-level: `schema_version`, `pass`, `pdf`, `pages[]`, `skipped_pages[]`, `errors[]`.

Per-page (`pages[]`): `page` (1-based), `status` (`ocr` | `skipped_has_text` | `failed`), `text`, `lines[]` with `{text, confidence, bbox}` in **PDF user space** (bottom-left origin).

BBox mapping (image pixels → PDF points):

- `pdf_x = media_left + (pixel_x / image_width) * media_width`
- `pdf_y = media_bottom + (1 - (pixel_y + h) / image_height) * media_height` for axis-aligned rects from EasyOCR quads (use bounding rect of quad).

Y-flip: image origin top-left; PDF origin bottom-left per MediaBox.

## Surface order

1. M0 contract (this doc + schema)
2. Python `FrisketOcrService` + PyInstaller spec
3. Core gate + report types
4. `PdfTool ocr`
5. `OcrPlugin` + dock UI
6. WixInstaller + smoke test
7. Unit tests (mock sidecar; no full EasyOCR in default CI)
