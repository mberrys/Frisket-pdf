# Tiered Bleed Preflight — Implementation Plan

Status: **draft** (MIC-151, Milestone 0).  
Scope: Frisket-pdf / PDF4QT 1.6.0.0.  
Related: MIC-134 (Tier-1 `bleed` check), MIC-121/122 (`PDFBleedFixup` / `add-bleed`), MIC-154 (PreflightEngine refactor), MIC-157 (PDFDocumentSession), MIC-158 (content-bleed check), MIC-159 (PDFBleedMarginProbe).

## Goal / Non-goals

- **Tier-1** (`bleed`): geometry box check. Is BleedBox ≥ TrimBox + amount_pt on all edges?
- **Tier-2** (`content-bleed`): content-aware check. Does rendered artwork extend into the bleed margin?
- Tier-2 does **NOT** mutate the PDF. `PDFBleedFixup::apply` is the consumer, not part of this check.
- No full-page raster in default mode.

## Architecture

| Layer | Class | Role |
|-------|-------|------|
| Core | `PDFDocumentSession` | Compile + decoded-stream cache |
| Core | `PreflightEngine` | Profile → findings orchestrator |
| Core | `PDFBleedMarginProbe` | Partial strip raster for bleed analysis |
| Core | `content-bleed` check | Tier-2 bleed content validation |
| PdfTool | `preflight` command | Thin driver calling PreflightEngine |

## Class names & APIs

```cpp
// PDFDocumentSession
class pdf::PDFDocumentSession
{
public:
    explicit PDFDocumentSession(pdf::PDFDocument* document);
    const PDFPrecompiledPage* compilePage(size_t pageIndex);
    QByteArray getDecodedStream(PDFObjectReference ref);
    void invalidate();
    bool isValid() const;
};

// PreflightEngine
class pdf::PreflightEngine
{
public:
    struct CheckDescriptor { QString id; std::function<void()> runner; };
    explicit PreflightEngine(PDFDocumentSession* session);
    QJsonObject run(const QJsonObject& profile);
};

// PDFBleedMarginProbe
class pdf::PDFBleedMarginProbe
{
public:
    struct Settings
    {
        int dpi = 150;
        int threshold = 16;
        QMarginsF margin;
        PDFBleedFixupSettings::ReferenceBox referenceBox =
            PDFBleedFixupSettings::ReferenceBox::TrimBox;
    };
    struct EdgeResult
    {
        bool hasContent = false;
        int inkPixels = 0;
        int totalPixels = 0;
    };
    struct Result
    {
        EdgeResult left, top, right, bottom;
        bool allEdgesCovered() const;
    };
    Result probe(const PDFPage* page, size_t pageIndex, const Settings& settings);
    // Fast path: use compiled page geometry bounds
    Result probeFast(const PDFPage* page, size_t pageIndex, const Settings& settings);
};
```

## Tier-1 / Tier-2 flow

```
for each page:
    tier1 = runBleedBoxCheck(page, profile)
    if tier1 failed:
        emit finding(type: "bleed", severity: error)
        mark fixup: add-bleed
        continue  // (skip Tier-2 for this page)
    if profile has content-bleed check enabled:
        tier2 = runContentBleedCheck(page, profile)
        if tier2 gaps found:
            emit finding(type: "content-bleed", severity: per-profile)
            mark fixup: add-bleed
```

## Content-bleed algorithm

1. **Fast bounds:** extract visible content bounds from compiled page (vector geometry, no raster)
2. Compare content bounds against bleed-margin strips (reference box expanded by amount_pt on each side)
3. If content bounds don't cover a strip → flag that side
4. If `raster_confirm: true` and fast path flagged a gap → render that side's strip at probe_dpi, threshold pixels → confirm/suppress

## PDFBleedMarginProbe spec

- Per-side strip definition: edgeStripSourceRect / edgeStripDestRect from `pdfbleedfixup.h` (reuse)
- Coordinate system: PDF user space, unrotated, reference-box lower-left origin
- Render features: Antialiasing + TextAntialiasing; ClipToCropBox off, DisplayAnnotations off
- CMS: use document CMS
- OCR: off

## Report schema

- No new top-level fields. `schema_version` stays 1.
- New finding `type`: `content-bleed`. `check_id`: `content-bleed`.
- `bbox` on content-bleed finding = the bleed-margin strip rect that lacks artwork.
- `fixups_available` entries may now include `params` object (additionalProperties). add-bleed entry params: `{ "mode": "mirror" }`.

## Dynamic fixups_available algorithm

```
result = profile.fixups[]
if bleed_failed OR content_bleed_gaps:
    result += {id: "add-bleed", safe: false, description: "...", params: {mode: "mirror"}}
if no_bleed_or_content_gaps:
    omit add-bleed from result
other fixups (rgb-to-cmyk, downsample-images) always include
```

## Profile flags for content-bleed

```json
{
  "id": "content-bleed",
  "severity": "warning",
  "amount_pt": 9,
  "raster_confirm": true,
  "probe_dpi": 150,
  "probe_threshold": 16
}
```

## Box policy

Same ISO nesting as MIRROR_BLEED_PLAN.md. Reference box = TrimBox (fallback CropBox → MediaBox).

## Surface order and relation to fixup

(per AGENTS.md / PLANNING.md)

- PdfTool preflight first (non-interactive harness)
- Editor plugin consumes report JSON (already shells out to PdfTool)
- PageMaster may batch-invoke add-bleed based on preflight findings
- add-bleed fixup is the consumer of the fixups_available signal

## Profile params for content-bleed

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `raster_confirm` | bool | false | When true, run strip-raster confirmation on pages flagged by fast bounds pass |
| `probe_dpi` | int | 150 | DPI for the strip-raster probe |
| `probe_threshold` | int | 16 | Pixel channel threshold (0–255) that counts as ink |

## Test / fixture plan

| Fixture ID | Expected pass | check_ids | Purpose |
|---|---|---|---|
| `content-bleed-missing` | false | `["content-bleed"]` | Boxes adequate, artwork stops at trim |
| `content-bleed-adequate` | true | `[]` | Boxes adequate, artwork extends into all bleed margins |
| `content-bleed-raster-confirm` | false | `["content-bleed"]` | Fast bounds flags gap; strip raster confirms missing content |

## Appendix: Sinalite-style profile example

```json
{
  "schema_version": 1,
  "name": "Sinalite Tiered Bleed",
  "checks": [
    { "id": "bleed", "required": true, "amount_pt": 9, "severity": "error" },
    { "id": "content-bleed", "amount_pt": 9, "severity": "warning", "raster_confirm": false }
  ],
  "fixups": [
    { "id": "add-bleed", "amount_pt": 9, "confirm": true }
  ]
}
```
