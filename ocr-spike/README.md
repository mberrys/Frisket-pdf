# MIC-128: EasyOCR spike (outside C++ tree)

Side-by-side evaluation of **EasyOCR** as an alternative to the planned **Unlimited-OCR** service ([MIC-123](https://linear.app/mbx2/issue/MIC-123/stand-up-the-ocr-service)). Nothing here links into Pdf4QtLibCore or vcpkg.

## Quick start

```bash
cd ocr-spike
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Fixtures (Python rasters) OR PdfTool render when built:
./prepare_samples.sh

python run_easyocr_spike.py
```

Results land in `results/spike_report.json`. See **[FINDINGS.md](FINDINGS.md)** for conclusions.

## With PdfTool (preferred for real job PDFs)

When `build/usr/bin/PdfTool` exists (or set `PDFTOOL=`):

```bash
export PDFTOOL=../build/usr/bin/PdfTool
./prepare_samples.sh   # render @ 300 DPI + fetch-text baseline
python run_easyocr_spike.py
```

`prepare_samples.sh` uses:

- `PdfTool render --image-res-mode dpi --image-res-dpi 300`
- `PdfTool fetch-text` for live-text baseline (compare manually or extend manifest)

Drop client/job PDFs into `fixtures/pdfs/` and re-run.

## Layout

| Path | Purpose |
|------|---------|
| `generate_fixtures.py` | Synthetic 300 DPI rasters + `fixtures/manifest.json` ground truth |
| `prepare_samples.sh` | PdfTool render/fetch-text when binaries available |
| `run_easyocr_spike.py` | EasyOCR driver; MIC-123 JSON shape |
| `FINDINGS.md` | Spike conclusions for OCR Module milestone |
| `fixtures/rasters/` | PNG inputs (generated; gitignored except via regenerate) |
| `results/spike_report.json` | Machine-readable metrics |

## OCR service contract (MIC-123)

```json
{
  "page": 1,
  "text": "...",
  "boxes": [[x0, y0, x1, y1]],
  "confidence": [0.95],
  "elapsed_ms": 10898
}
```

EasyOCR also returns per-detection `detections[]` for debugging.

## Not in scope

- FastAPI wrapper (MIC-123)
- Per-page OCR gating (MIC-124)
- C++ / Editor integration
