# Mirror Bleed — Implementation Plan

Status: **approved with changes** (Senior Developer review, 2026-07-09).  
Scope: Frisket-pdf / PDF4QT 1.6.0.0.  
Primary API name: `PDFMirrorBleed` (not `PDFBleedGenerator`).

## Goal

Add a fixup that expands page boxes and **mirrors edge artwork outward** into the bleed area when a PDF has no (or insufficient) bleed. Approach is **raster mirror** (pragmatic print-shop fixup), not PDF/X-native prepress.

## Non-goals

- Extending `PDFPageGeometry` to generate bleed artwork (boxes / scale only)
- Using `PDFDocumentManipulator` crop margins as bleed
- Vector content analysis / live-path bleed
- Making Viewer or PageMaster a plugin host
- Interactive confirm UX in PdfTool (CLI stays non-interactive)

## Architecture

| Layer | Role | Milestone |
|-------|------|-----------|
| **Core** `PDFMirrorBleed` | Settings, report, `apply()`, strip math, raster mirror | M1 |
| **PdfTool** `mirror-bleed` | First consumer / validation harness | M1 |
| **UnitTests** | Rect / skip math; optional fixture later | M2 |
| **PageMaster** | Batch export + JSON settings | M3 |
| **Editor** (optional) | Single-doc confirm + action | M4 |

```text
assemble (PageMaster)
    -> PDFPageGeometry::apply   (establish final trim / page size)
    -> PDFMirrorBleed::apply    (extend outward from post-geometry Trim/Crop)
    -> image optimize / write
```

Shared PDF logic lives in Core. Surfaces call Core; they do not reimplement strip math.

---

## Phase 0 — Spec locks (do before coding)

These decisions are **locked** unless a later PR explicitly revises them.

### Reference box

- Default: **TrimBox**
- Fallback: **CropBox** when Trim is absent or equals the loader default (Bleed/Trim/Art default to CropBox in `pdfpage.cpp`)

### Bleed amount

- Uniform mm **and** optional per-side `QMarginsF` (L, T, R, B)
- Default: **3 mm** all sides
- Side with 0 mm: skip that side (no degenerate strip)

### Box policy (default)

ISO-friendly nesting: `MediaBox >= CropBox >= BleedBox >= TrimBox`.

| Box | Default action |
|-----|----------------|
| **TrimBox** | **Keep fixed** (`expandTrimBox = false`) |
| **ArtBox** | Leave unchanged |
| **BleedBox** | Expand to target bleed rect |
| **CropBox** | Expand to same target as BleedBox |
| **MediaBox** | Expand to cover target bleed rect |

**Do not** expand BleedBox past CropBox while leaving CropBox unchanged — invalid / viewer-hostile.

Advanced “strict CropBox” (leave CropBox fixed) is opt-in only and must be documented as non-default.

### Per-page operation order (blocker)

1. Compute reference + target bleed rects (unrotated page user space)
2. Apply skip heuristic per side (unless `--force`)
3. **Write expanded MediaBox / CropBox / BleedBox first**
4. Then `PDFPageContentStreamBuilder` (`CoordinateSystem::PDF`, `Mode::PlaceBefore`) + `drawImage`
5. Finalize modifier

`PDFPageContentStreamBuilder::begin()` sizes the paint surface from the **current** MediaBox. Expanding after paint puts destinations off-canvas.

### Content order

- **`PlaceBefore`** — mirrored strips under existing page streams (visible only outside trim)
- Not `PlaceAfter` (that is for overlays; see SignaturePlugin)

### Corners (M1)

- **Overlap adjacent strips** at corners
- Dedicated corner tiles deferred unless seams are unacceptable

### Skip heuristic

- Box-based only: if existing BleedBox already provides >= target margin on a side, skip that side
- **Does not** detect real edge artwork
- PdfTool / settings: `--force` (or equivalent flag) overrides skip

### Render defaults

| Setting | Default |
|---------|---------|
| DPI | 300 |
| `ClipToCropBox` | **Off** |
| `DisplayAnnotations` | **Off** (do not mirror annotation chrome) |
| Antialiasing | On (match sensible `PDFRenderer::Features` subset) |
| CMS / font cache / OC | Owned inside Core `apply()` (or required inputs), same pattern as `pdftoolredact` / image compressor |

### Raster strategy (M1)

There is **no** `renderPageRegion` API today (`PDFRasterizer` maps full rotated MediaBox).

- **M1:** full-page raster at DPI → crop edge strips in `QImage` → mirror → place
- **Later:** optional strip-matrix / region render for RAM

### PageMaster op order (locked)

1. `assemble`
2. `PDFPageGeometry::apply` (final trim / page size)
3. `PDFMirrorBleed::apply`
4. image optimize / write

“Mirror then geometry” is advanced/opt-in only. If geometry `applyBleedBox` fights mirror settings, warn or disable the conflicting combo in UI.

### Rotation

- Compute strips in **unrotated page user space**
- Use existing render matrices for `/Rotate`
- Explicit test cases for 90 / 180 / 270

### Destructive rewrite UX

| Surface | Behavior |
|---------|----------|
| PdfTool | Non-interactive: `--report`, `--force`, optional `--dry-run`, console format text/xml/html |
| PageMaster | Confirm / warning when mirror bleed enabled on export |
| Editor | Confirm dialog before apply (Phase 6) |

---

## Phase 1 — Core + PdfTool (M1)

Merge former “Phase 1 + Phase 2”: one Core deliverable, then PdfTool.

### New files

- `Pdf4QtLibCore/sources/pdfmirrorbleed.h`
- `Pdf4QtLibCore/sources/pdfmirrorbleed.cpp`
- Wire `Pdf4QtLibCore/CMakeLists.txt`
- `PdfTool/pdftoolmirrorbleed.h` / `.cpp`
- Wire `PdfTool/CMakeLists.txt` (static registration like other tools)

### API sketch

```cpp
struct PDFMirrorBleedSettings
{
    QString pageRange = "-";

    enum class ReferenceBox { CropBox, TrimBox, MediaBox };
    ReferenceBox referenceBox = ReferenceBox::TrimBox;

    QMarginsF bleedMM = QMarginsF(3, 3, 3, 3); // L, T, R, B

    bool expandMediaBox = true;
    bool expandCropBox = true;
    bool expandBleedBox = true;
    bool expandTrimBox = false;

    int dpi = 300;
    bool skipIfAlreadyBleeding = true;
    bool force = false;

    PDFRenderer::Features renderFeatures; // no ClipToCropBox, no DisplayAnnotations
};

struct PDFMirrorBleedReport
{
    // per-page: original boxes, new boxes, sides applied, skip reasons
};

class PDFMirrorBleed
{
public:
    static PDFOperationResult apply(PDFDocument* document,
                                    const PDFMirrorBleedSettings& settings,
                                    PDFMirrorBleedReport* report = nullptr);
};
```

Match `PDFPageGeometry::apply` style. Use `PDFDocumentModifier` + `markPageContentsChanged()` + `finalize()`.

### Internal helpers (same milestone)

| Helper | Responsibility |
|--------|----------------|
| `referenceRect` | Trim/Crop/Media + fallback |
| `targetBleedRect` | Expand by bleed mm (points) |
| `edgeStripSourceRect` / `edgeStripDestRect` | Sample vs place regions |
| `renderFullPage` + crop | M1 raster path |
| `mirrorForSide` | H/V mirror |
| `placeStrip` | `PlaceBefore` + `drawImage` |

### PdfTool CLI sketch

```text
PdfTool mirror-bleed [options] input.pdf
  --output / -o out.pdf
  --bleed-mm 3
  --bleed-mm-ltrb 3,3,3,3
  --reference-box trim|crop|media
  --dpi 300
  --page-first / --page-last / --page-select
  --pswd
  --force
  --dry-run
  --report
  --console-format text|xml|html
```

Flow: `readDocument` → `PDFMirrorBleed::apply` → write (unless dry-run) → optional report.

### M1 acceptance

- [ ] Core `apply` expands Media/Crop/Bleed; Trim unchanged by default
- [ ] MediaBox expanded **before** content placement
- [ ] Mirrored strips visible outside trim when rendered
- [ ] `PlaceBefore` used
- [ ] PdfTool writes output; `--report` shows before/after boxes
- [ ] `--force` overrides skip; skip is documented as box-only
- [ ] No misuse of `PDFPageGeometry` as the content path

---

## Phase 2 — Unit tests (M2)

**Location:** `UnitTests/` (Qt Test), wire like existing targets.

**Priority:**

1. Rect expansion math (known trim + 3 mm → expected boxes)
2. Skip heuristic (sufficient BleedBox → side skipped; force → not skipped)
3. Optional later: tiny fixture PDF (colored edge) + box assertions / pixel sample

Do not block M1 ship on a full render fixture.

---

## Phase 3 — PageMaster (M3)

**Touch:** `Pdf4QtPageMaster/mainwindow.h/.cpp` (and dialog/UI as needed).

- Persist `mirrorBleed` settings beside `pageGeometry` in workspace / export JSON
- Export order: assemble → geometry → **mirror bleed** → optimize → write
- Warn when geometry `applyBleedBox` conflicts with mirror settings
- Export confirmation when mirror bleed is enabled

### JSON sketch

```json
"mirrorBleed": {
  "enabled": true,
  "bleedMM": { "left": 3, "top": 3, "right": 3, "bottom": 3 },
  "referenceBox": "trim",
  "dpi": 300,
  "skipIfAlreadyBleeding": true,
  "expandCropBox": true
}
```

---

## Phase 4 — Editor + docs polish (M4)

- Optional Editor action or thin plugin calling Core; confirm dialog
- Short user/dev note: raster limits (DPI, file size, spot/overprint flatten)
- Keep `NOTES.txt` §14.11 honest: still not full prepress; this is a raster fixup
- Optional: bleed-deficit reporting via `info-page-boxes` or tool report

---

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Invalid box nesting | Default expand Media+Crop+Bleed; Trim fixed |
| Off-canvas strips | Expand MediaBox before `begin()` |
| Geometry undoes / rescales bleed | PageMaster: geometry then mirror |
| Double art on already-bleeding files | Box skip + `--force`; document limits |
| No region render API | Full-page + crop for M1 |
| Rotation bugs | Unrotated strip math; test 90/180/270 |
| Spot colors / overprint | Document as raster quality limit |
| Large files | Default 300 DPI; strip crop after full render |
| Corners | Overlap strips in M1 |

---

## Milestone summary

| Milestone | Deliverable |
|-----------|-------------|
| **M1** | Phase 0 locks + Core `PDFMirrorBleed` + PdfTool `mirror-bleed` |
| **M2** | Rect / skip unit tests |
| **M3** | PageMaster JSON + export hook |
| **M4** | Editor (optional) + docs |

## Implementation order (first slice)

1. Land this plan (Phase 0 locks).
2. Core strip/box math + unit tests (no raster yet) — proves nesting and skip.
3. Full-page render → mirror → `PlaceBefore` inside Core `apply`.
4. PdfTool `mirror-bleed` as validation harness.
5. PageMaster only after M1 is proven.
6. Optimize strip rendering / Editor later.

## Related code (anchors)

| Area | Path |
|------|------|
| Box getters | `Pdf4QtLibCore/sources/pdfpage.h` |
| Box defaults | `Pdf4QtLibCore/sources/pdfpage.cpp` |
| Box setters / content streams | `Pdf4QtLibCore/sources/pdfdocumentbuilder.h` / `.cpp` |
| Geometry (not the solution) | `Pdf4QtLibCore/sources/pdfpagegeometry.*` |
| Renderer / ClipToCropBox | `Pdf4QtLibCore/sources/pdfrenderer.*`, `pdfpainter.cpp` |
| PlaceAfter pattern | `Pdf4QtEditorPlugins/SignaturePlugin/signatureplugin.cpp` |
| PageMaster export | `Pdf4QtPageMaster/mainwindow.cpp` |
| Headless render setup | `PdfTool/pdftoolredact.cpp`, `pdftoolrender.cpp` |
| Prepress note | `NOTES.txt` §14.11 |
