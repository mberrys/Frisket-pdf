# Tiered Bleed Preflight — Design Plan (M0)

Status: **draft — M0, pending review** (locks names before Core code; 2026-07-10).
Scope: Frisket-pdf / PDF4QT 1.6.0.0. Phase 1 — CLI engine.
Primary API names: **`PDFDocumentSession`** (`pdfdocumentsession.*`), **`PDFBleedMarginProbe`** (`pdfbleedmarginprobe.*`), **`PreflightEngine`** (PdfTool orchestrator).
Finding types: **`content-bleed`**, **`bleed-margin-empty`**, **`needs-auto-bleed`**.
Profile param: **`raster_confirm`** (per-check bool).
Epic: [MIC-151](https://linear.app/mbx2/issue/MIC-151). This doc: [MIC-152](https://linear.app/mbx2/issue/MIC-152).
Related: MIC-153 (`PDFDocumentSession`), MIC-155 (`PDFBleedMarginProbe`), MIC-137/136/138 (plugin + overlays), MIC-121/122/141 (`add-bleed` fixup consumers), MIC-134 (box `bleed` check, done).

## Goal

Add a **two-tier bleed preflight** that catches artwork which stops short of the bleed edge — the failure a pure box-geometry `bleed` check (MIC-134) cannot see, because a page can carry a correct `BleedBox` while the *artwork* leaves a white gap inside it.

- **Tier 1 (structural, always on, no raster):** box geometry + a content-bounds test. Compute the bounding box of drawn marks on the page and compare it to the trim/bleed geometry. Cheap, deterministic, runs on every page.
- **Tier 2 (raster strip probe, opt-in):** only when the profile sets `raster_confirm: true` **and** Tier 1 flagged the page, rasterize a thin strip of the bleed margin at low DPI to confirm whether the margin is actually empty (vs. filled by content Tier 1 could not bound, e.g. a clipped or soft-masked image).

This mirrors Acrobat/Sinalite behavior: flag most bleed gaps structurally, escalate to pixels only where structure is ambiguous, and never pay full-page raster cost on the default path.

### Non-goals

- **Generating** bleed artwork. That is `PDFBleedFixup` / `PdfTool add-bleed` (MIC-121/122) — see [MIRROR_BLEED_PLAN.md](MIRROR_BLEED_PLAN.md). This feature *detects* and may *advertise* `add-bleed` as a fixup; it never paints.
- Changing the shipped `frisket-default` profile's pass/fail behavior. Without `raster_confirm`, Tier 2 never runs and the default profile stays exactly as fast as today.
- Async / GUI page compilation. The engine path is synchronous and headless (`PDFRenderer::compile`), not the Editor's `PDFAsynchronousPageCompiler`.
- New toolchains or dependencies. This stays on Pdf4QtLibCore (MIT) + QPDF (Apache-2.0); no JRE, Ghostscript, PDFBox, or PikePDF (per the hybrid sidecar plan).

### Architecture alignment

Consistent with the **hybrid sidecar** plan of record: all preflight logic lives in the **engine** (`Pdf4QtLibCore` + the PdfTool `preflight` command, a separate process), never inside the Editor GUI process. `PDFDocumentSession` and `PDFBleedMarginProbe` are **reuse-extensions** built on the existing `PDFRenderer` / `PDFPrecompiledPage` stack — a shared perf foundation, not a parallel renderer. Because they use the same document model as the PDF4QT renderer, the `bbox`es they emit match what Phase 2 overlays (`IDocumentDrawInterface`) draw — the core reason the engine is C++/PdfTool rather than Java.

## Architecture

| Layer | Role | Issue |
|-------|------|-------|
| **Core** `PDFDocumentSession` | Owns document + caches (compiled page, decoded stream, font, CMS); reused by engine and, later, Editor | MIC-153 |
| **Core** `PDFBleedMarginProbe` | Tier-2 partial-strip raster over a session; returns margin-empty verdict + bbox | MIC-155 |
| **Core** `pdftool::preflight` (header-only math) | Tier-1 content-bounds test; existing box math | issue 4 |
| **PdfTool** `PreflightEngine` | Orchestrates checks over one session; assembles report; dynamic `fixups_available` | issue 3 / 6 |
| **PdfTool** `preflight` command | Non-interactive harness; PDF + profile → JSON report on stdout | existing |
| **Editor** (P2 stretch) | Reuses `PDFDocumentSession`; not this epic's core | MIC-159 |

```text
PdfTool preflight in.pdf --profile p.json
    -> PreflightEngine(session)
        -> Tier 1: box bleed (bleedAdequate) + content-bounds (contentWithinBleed)   [no raster]
        -> Tier 2: PDFBleedMarginProbe strip raster   [only if raster_confirm && Tier 1 flagged]
    -> report JSON (errors/warnings + dynamic fixups_available) -> stdout
```

## Tier-1 vs Tier-2 trigger matrix

Tier 1 always runs. Tier 2 runs **only** when the check's profile entry sets `raster_confirm: true` **and** Tier 1 flagged the page.

| Signal | Tier | Raster? | Emits |
|--------|------|---------|-------|
| `BleedBox` missing / smaller than `amount_pt` beyond trim | 1 | no | `content-bleed` (box-level), + `needs-auto-bleed` |
| Content bbox does not reach `amount_pt` into bleed on some edge | 1 | no | `content-bleed` (content-level) |
| Tier-1 flagged **and** `raster_confirm:false` (or unset) | — | no | Tier-1 finding only; Tier 2 skipped |
| Tier-1 flagged **and** `raster_confirm:true`, strip has content | 2 | strip | (downgrade) no `bleed-margin-empty`; content actually bleeds |
| Tier-1 flagged **and** `raster_confirm:true`, strip empty | 2 | strip | `bleed-margin-empty` (confirmed), + `needs-auto-bleed` |

Truth table for the Tier-2 gate:

| Tier-1 result | `raster_confirm` | Tier 2 runs? | Net finding |
|---------------|------------------|--------------|-------------|
| pass | any | no | none |
| flag | absent / false | no | `content-bleed` (structural, unconfirmed) |
| flag | true | yes | `bleed-margin-empty` if strip empty, else cleared |

Rationale: the structural Tier-1 finding is enough for a report; `raster_confirm` buys a pixel-level confirmation for shops that want to suppress false positives from content Tier 1 cannot bound. The default profile omits `raster_confirm`, so its behavior and cost are unchanged.

## Finding types

All `type`s are kebab-case per `report.schema.json` (`^[a-z][a-z0-9-]*$`) and carry the mandatory `bbox`.

### `content-bleed`
- **Severity:** `error` (default; profile-overridable).
- **Tier:** 1 (structural). May be corroborated or cleared by Tier 2.
- **When:** the effective content extent does not cover the required bleed reach (`amount_pt` beyond the trim box) on one or more edges — whether because the `BleedBox` is short or because the artwork itself stops inside it.
- **bbox:** the trim/bleed region on the offending edge(s); falls back to the effective box when a single edge cannot be isolated.
- **Fixup:** contributes `needs-auto-bleed` → dynamic `add-bleed`.

### `bleed-margin-empty`
- **Severity:** `warning` (default). Tier-2 confirmation of a Tier-1 flag.
- **Tier:** 2 (raster). Emitted only when `raster_confirm:true` and the probed strip is empty within threshold.
- **When:** the rasterized bleed-margin strip contains no meaningful content (coverage/luminance below threshold), confirming the margin is genuinely white.
- **bbox:** the probed strip rectangle in page user space.
- **Fixup:** contributes `needs-auto-bleed`.

### `needs-auto-bleed`
- **Severity:** `info` (advisory; drives a fixup rather than failing the run).
- **Tier:** derived — emitted whenever `content-bleed` or `bleed-margin-empty` fired for a page.
- **When:** the document is a candidate for the `add-bleed` fixup (MIC-121/122/141).
- **bbox:** the page media/trim box (page-level advisory).
- **Fixup:** the presence of this finding is what makes `PreflightEngine` list `add-bleed` in `fixups_available` **dynamically** (see below).

### bbox coordinate convention — LOCK

Seam to resolve now so new findings do not entrench it: `report.schema.json` `$defs.bbox` documents `[x0, y0, x1, y1]` with the **media-box lower-left origin (y-up)**, but the current emitter (`pdftoolpreflight.cpp` `rectToBbox`) writes `[left, top, right, bottom]` straight from a Qt `QRectF` (top-left origin).

**Lock:** keep the existing emitter behavior (`QRectF` order) as the single source of truth for all findings — changing it would silently move every existing overlay — and **correct the schema description** to match (`[x0, y0, x1, y1]` where the values are the `QRectF` `left/top/right/bottom` in the page's top-left-origin user space). New Tier-1/Tier-2 findings must produce `bbox`es in that same convention. Track the schema-comment fix as part of issue 4.

### Dynamic `fixups_available` — LOCK

Today `fixups_available[]` is populated **statically** from `profile.fixups` in `buildReportJson`, with `safe` hard-coded `false`. This epic makes it **finding-driven**: `PreflightEngine` lists a profile fixup only when a finding warrants it — `add-bleed` appears when any `needs-auto-bleed` finding fired. Fixups remain opt-in and confirm-required; this changes *advertising*, not *applying*. (`safe` stays `false` for `add-bleed`.)

## Box policy (reuse `pdftoolpreflightchecks.h`)

All new geometry math lands in the existing **header-only, PDF-free** `pdftool::preflight` namespace so the command (`pdftoolpreflight.cpp`) and the unit test (`tst_preflightchecks.cpp`) share one implementation. Page-box extraction stays in the command; only rectangle math lives in the header.

- Reuse `resolveEffectiveBox(trim, crop, media)` for the trim → crop → media fallback.
- Reuse `bleedAdequate(trim, bleed, amountPt, tolerancePt)` for the box-geometry half of Tier 1.
- **New helper** for the content-bounds half:

```cpp
// Tier-1 content test: does the drawn-content bounding box reach amountPt into the
// bleed region beyond the trim box, on every edge, within tolerancePt? A content
// box that stops short of the required reach on any edge is NOT within bleed.
inline bool contentWithinBleed(const QRectF& contentBounds,
                               const QRectF& trim,
                               const QRectF& bleed,
                               qreal amountPt,
                               qreal tolerancePt);
```

- **Tolerance lock:** the existing `bleed` check hard-codes a `0.25` pt tolerance in `isBleedAdequate`. This epic makes bleed tolerance **profile-driven** via `tolerance_pt` (already used by `trim`/`page-size`), defaulting to the current `0.25` pt when unset, so the content and box tests share one configurable tolerance.

## `PDFDocumentSession` API sketch (MIC-153)

A headless Core session that owns a document plus the caches the tiered checks need. Grounded in the existing wiring at `pdftoolrender.cpp:178` and `pdfbleedfixup.cpp:464` (which already assemble document + OC activity + CMS + font cache + renderer — the session is that wiring plus ownership and caching).

```cpp
class PDF4QTLIBCORESHARED_EXPORT PDFDocumentSession
{
public:
    explicit PDFDocumentSession(const PDFDocument* document,
                                PDFRenderer::Features features = PDFRenderer::None,
                                const PDFMeshQualitySettings& meshQuality = {});

    const PDFDocument&      document() const;
    const PDFFontCache&     fontCache() const;
    const PDFCMS*           cms() const;

    // compile-on-miss, cached; repeat access returns the same pointer.
    const PDFPrecompiledPage* compiledPage(PDFInteger pageIndex);

    // filter-once decode, LRU-cached by object reference.
    const QByteArray&       decodedStream(PDFObjectReference streamRef);
};
```

Backing primitives (all confirmed present in `Pdf4QtLibCore/sources/`):

- Owns `PDFOptionalContentActivity` (`OCUsage::Export`), a `PDFCMSManager` (→ `PDFCMSPointer`), and `PDFFontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT)` with `setCacheShrinkEnabled(nullptr, false)` — exactly the `pdfbleedfixup.cpp:464` pattern.
- `compiledPage()` compiles via `PDFRenderer::compile(PDFPrecompiledPage*, pageIndex)` into a `QCache<PDFInteger, PDFPrecompiledPage>`, reusing `PDFPrecompiledPage::markAccessed()` / `hasExpired()` for LRU (the same hooks the Editor's `PDFAsynchronousPageCompiler` uses).
- `decodedStream()` wraps `PDFObjectStorage::getDecodedStream` (via `PDFDocument::getDecodedStream`), keyed by `PDFObjectReference`. **No decode cache exists today** — this LRU is genuinely new (used by Tier-1 content-bounds and Tier-2 to avoid re-filtering the same image/content stream).

**Tier-1 content bounds** come from the compiled page: derive the union of drawn-mark bounds from the `PDFPrecompiledPage` instruction list, in page user space, and feed it to `contentWithinBleed`.

**`PDFBleedMarginProbe` (MIC-155)** takes a `PDFDocumentSession&`, a page index, and a strip rectangle, and rasterizes only that strip via `PDFRenderer::render(QPainter*, const QTransform&, pageIndex)` with `ClipToCropBox` **off** — the strip pattern already implemented in `pdfbleedfixup.cpp` (~585–661), which today rasterizes full-page then crops. Generalizing that into a true sub-rect render is the P3 stretch (MIC-158); the probe can start with the full-page-then-crop approach and swap later without changing its signature.

## DPI / threshold defaults

Locked here (not in AGENTS.md) per `docs/PLANNING.md`. All reviewable in this M0.

| Setting | Default | Notes |
|---------|---------|-------|
| Tier-2 probe DPI | **150** | Half the 300-DPI print minimum; enough to detect empty vs. filled margin, ~¼ the pixels of a 300-DPI probe. |
| `bleed-margin-empty` coverage threshold | **≥ 0.5% non-background pixels** ⇒ not empty | Coverage of the strip; tuned in unit tests against fixtures. |
| Bleed reach | `amount_pt` = **9** | Existing `frisket-default` value (9 pt ≈ 0.125 in). |
| Bleed/content tolerance | `tolerance_pt` = **0.25** when unset | Promotes today's hard-coded `isBleedAdequate` value to a profile param. |
| Render features | `ClipToCropBox` off, annotations off | Match `pdfbleedfixup.cpp` strip render; probe the full bleed margin, not the crop. |

Profile example (Sinalite-style, opts into Tier 2):

```json
{
  "id": "bleed",
  "required": true,
  "amount_pt": 9,
  "tolerance_pt": 0.25,
  "raster_confirm": true,
  "severity": "error"
}
```

`raster_confirm` fits `profile.schema.json`'s check objects (which allow `additionalProperties: true`); register it in the schema and the README check-params table alongside `amount_pt` / `min_dpi` / `allowed`.

## Editor / canvas reuse notes (MIC-137)

`PDFDocumentSession` is designed so the Editor can later delegate to one Core session instead of the widget-bound split it uses today:

- The Editor's de-facto session is spread across `PDFDrawSpaceController` (owns `PDFFontCache`, holds document + `PDFOptionalContentActivity`) and `PDFDrawWidgetProxy` (owns the `PDFAsynchronousPageCompiler` with its `QCache<PDFInteger, PDFPrecompiledPage>`, exposes `getCMSManager`).
- `PDFDocumentSession` mirrors that getter surface (`document()`, `fontCache()`, `cms()`, `compiledPage()`) so `PDFDrawWidgetProxy` could hold or delegate to a session without changing its callers.
- The Frisket Preflight plugin (MIC-137, done) currently only displays report JSON produced by the CLI. Once the session exists, an in-process preflight run in the Editor could share the already-compiled pages of the open document instead of recompiling — the payoff of putting the cache in Core.
- **Actual Editor wiring is the P2 stretch (MIC-159 / issue 8), not this epic's core.** This doc only locks the session's shape so that reuse stays possible.

## Related code

| Area | Path |
|------|------|
| Page boxes | `Pdf4QtLibCore/sources/pdfpage.h` (`getMediaBox/getCropBox/getBleedBox/getTrimBox`) |
| Renderer / compile | `Pdf4QtLibCore/sources/pdfrenderer.h`, `pdfpainter.h` (`PDFPrecompiledPage`) |
| Session prototype | `Pdf4QtLibCore/sources/pdfbleedfixup.cpp` (wiring + strip raster) |
| Headless wiring example | `PdfTool/pdftoolrender.cpp` |
| Decoded streams | `Pdf4QtLibCore/sources/pdfdocument.h`, `pdfstreamfilters.h` |
| Editor session split | `Pdf4QtLibWidgets/sources/pdfdrawspacecontroller.h`, `pdfcompiler.h` |
| Preflight command | `PdfTool/pdftoolpreflight.cpp` |
| Preflight box math | `PdfTool/pdftoolpreflightchecks.h` |
| Report / profile schema | `frisket-preflight/schemas/report.schema.json`, `profile.schema.json` |
| Default profile | `frisket-preflight/profiles/frisket-default.{yaml,json}` |
| Sibling fixup plan | `docs/MIRROR_BLEED_PLAN.md` |
| Prepress note | `NOTES.txt` §14.11 |
| Planning process | `docs/PLANNING.md` |
