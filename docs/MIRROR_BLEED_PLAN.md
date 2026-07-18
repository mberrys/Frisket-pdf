# Bleed Fixup — Implementation Plan

Status: **approved with changes** (Senior Developer + triple review, 2026-07-09).  
Scope: Frisket-pdf / PDF4QT 1.6.0.0.  
Primary API name: **`PDFBleedFixup`** (`pdfbleedfixup.*`) with mode enum (`Mirror`, `PixelRepeat`, `Stretch`).  
PdfTool command: **`add-bleed --mode ...`**.  
Related: MIC-121 (Mirror / M1), MIC-122 (PixelRepeat + Stretch / M2; blocked by MIC-121 scaffolding).

## Goal

Expand page boxes and fill the bleed margin by raster edge-extend:
- **Mirror** — reflect edge artwork outward
- **PixelRepeat** — tile outermost edge pixels
- **Stretch** — scale edge pixels across bleed depth

Pragmatic print-shop fixup, not PDF/X-native prepress.

## Non-goals

- Extending `PDFPageGeometry` to generate bleed artwork (boxes / scale only)
- Using `PDFDocumentManipulator` crop margins as bleed
- Vector content analysis / live-path bleed
- Making Viewer or PageMaster a plugin host
- Interactive confirm UX in PdfTool (CLI stays non-interactive)

## Architecture

| Layer | Role | Milestone |
|-------|------|-----------|
| **Core** `PDFBleedFixup` | Settings, report, `apply()`, strip math, modes | M1–M2 |
| **PdfTool** `add-bleed` | First consumer / validation harness | M1–M2 |
| **UnitTests** | Rect / skip / strip builders | M2 |
| **PageMaster** | Batch export + JSON settings | M3 |
| **Editor** (optional) | Confirm + single-doc action | M4 |

```text
assemble (PageMaster)
    -> PDFPageGeometry::apply   (establish final trim / page size)
    -> PDFBleedFixup::apply     (extend outward from post-geometry Trim/Crop)
    -> image optimize / write
```

## Phase 0 — Spec locks

### Reference box
- Default: **TrimBox**; fallback **CropBox** when Trim absent / loader-default

### Bleed amount
- Uniform mm and/or per-side `QMarginsF` (L,T,R,B); default **3 mm**; 0 mm skips that side

### Box policy (default)
ISO nesting: `MediaBox >= CropBox >= BleedBox >= TrimBox`.

| Box | Default |
|-----|---------|
| TrimBox | Keep fixed |
| ArtBox | Unchanged |
| BleedBox / CropBox / MediaBox | Expand to target bleed |

### Per-page order
1. Compute reference + target bleed (unrotated user space)
2. Skip sides (unless force)
3. Normalize / expand MediaBox (and Crop/Bleed) **before** paint
4. `PDFPageContentStreamBuilder` (`CoordinateSystem::PDF`, `PlaceBefore`) + `drawImage`
5. Finalize

### Sample edge
- Sample **inner reference-box edge** (not expanded Media outer edge)
- Place **outward** into bleed margin
- Mirror strip ≈ bleed depth at DPI; Repeat/Stretch use `samplePixels` (default 1)
- Stretch/Repeat: nearest-neighbor

### Other locks
- Corners M1: overlap side strips
- Skip: box-based only; `--force` overrides
- Render: 300 DPI; ClipToCropBox off; DisplayAnnotations off; CMS/font/OC in Core
- Raster: full-page → crop strips
- PageMaster: assemble → geometry → bleed fixup → optimize
- PdfTool: `--dry-run` / `--force` / `--report` (non-interactive)

## API

```cpp
enum class Mode { Mirror, PixelRepeat, Stretch };

struct PDFBleedFixupSettings { /* mode, pageRange, referenceBox, bleedMM,
  expand*Box, dpi, samplePixels, skipIfAlreadyBleeding, force, renderFeatures */ };

PDFBleedFixup::apply(PDFDocument*, settings, report*);
```

M1: enum + Mirror. M2: PixelRepeat + Stretch.

## PdfTool

```text
PdfTool add-bleed --mode mirror|pixel-repeat|stretch \
  --bleed-mm 3 [--bleed-mm-ltrb 3,3,3,3] --sample-pixels 1 \
  --reference-box trim|crop|media --dpi 300 \
  --force --dry-run --report -o out.pdf in.pdf
```

## PageMaster JSON

```json
"bleedFixup": {
  "enabled": true,
  "mode": "mirror",
  "bleedMM": { "left": 3, "top": 3, "right": 3, "bottom": 3 },
  "referenceBox": "trim",
  "dpi": 300,
  "samplePixels": 1,
  "skipIfAlreadyBleeding": true
}
```

## Related code

| Area | Path |
|------|------|
| Boxes | `pdfpage.h` / `pdfdocumentbuilder.h` |
| Geometry (not solution) | `pdfpagegeometry.*` |
| Renderer | `pdfrenderer.*` |
| PageMaster export | `Pdf4QtPageMaster/mainwindow.cpp` |
| Planning process | `docs/PLANNING.md` |
| Prepress note | `NOTES.txt` §14.11 |

## Relationship to preflight

`add-bleed` is the consumer of the dynamic `fixups_available` signal from two-tier
bleed preflight (MIC-151). When `PdfTool preflight` runs a profile with both Tier-1
(`bleed`) and Tier-2 (`content-bleed`) checks, the engine dynamically surfaces
`add-bleed` in `fixups_available` when:

- Tier-1 fails (BleedBox insufficient relative to TrimBox), **or**
- Tier-2 finds content gaps (artwork does not extend into the bleed margin, even
  though boxes are technically adequate)

This means the preflight report may surface `add-bleed` even when a casual box
check would pass — the content-aware Tier-2 closes that gap. PageMaster and the
Editor plugin consume this signal to offer the fixup without a separate analysis
step.
