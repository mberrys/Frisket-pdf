# Tiered Bleed Preflight — Design Plan (M0)

Status: **implemented** (Tier-1/Tier-2 shipped; residual raster golden = Linear MIC-325). M0 locks below remain authoritative.
Scope: Frisket-pdf / PDF4QT 1.6.0.0. Phase 1 — CLI engine.
Primary API names: **`PDFDocumentSession`** (`pdfdocumentsession.*`), **`PDFBleedMarginProbe`** (`pdfbleedmarginprobe.*`), **`PreflightEngine`** (PdfTool orchestrator).
Finding types: **`content-bleed`**, **`bleed-margin-empty`**, **`needs-auto-bleed`**.
Profile params: **`raster_confirm`** (bool), **`raster_confirm_dpi`** (default 150), **`raster_white_threshold`** (default 0.9975) — per-check.
Plan issue: [MIC-152](https://linear.app/mbx2/issue/MIC-152). Implementation: MIC-158/155/160. Residual golden: [MIC-325](https://linear.app/mbx2/issue/MIC-325).
**Note:** Linear [MIC-151](https://linear.app/mbx2/issue/MIC-151) is now the deferred *performance* epic — not this bleed feature.
Related: MIC-153 (`PDFDocumentSession`, deferred), MIC-155 (`PDFBleedMarginProbe`), MIC-137/136/138 (plugin + overlays), MIC-121/122/141 (`add-bleed` fixup consumers), MIC-134 (box `bleed` check, done).

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
| **Editor** (P2 stretch) | Reuses `PDFDocumentSession`; not this epic's core | MIC-156 |

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
| Tier-1 flagged **and** `raster_confirm:true`, every edge strip has content | 2 | 4 strips | (downgrade) no `bleed-margin-empty`; content actually bleeds on all sides |
| Tier-1 flagged **and** `raster_confirm:true`, one or more edge strips empty | 2 | 4 strips | `bleed-margin-empty` per empty edge, + `needs-auto-bleed` |

Truth table for the Tier-2 gate:

| Tier-1 result | `raster_confirm` | Tier 2 runs? | Net finding |
|---------------|------------------|--------------|-------------|
| pass | any | no | none |
| flag | absent / false | no | `content-bleed` (structural, unconfirmed) |
| flag | true | yes (all 4 edges probed) | `bleed-margin-empty` for each edge below the coverage threshold; edges above it are cleared |

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
- **Tier:** 2 (raster). Emitted only when `raster_confirm:true` and a probed **edge** strip is empty within threshold.
- **When:** one of the four bleed-margin edge strips (top/bottom/left/right, probed independently — see "Per-edge probing" below) contains no meaningful content, confirming that edge's margin is genuinely white.
- **Per-edge, not one finding per page:** a page can emit **up to four** `bleed-margin-empty` findings, one per empty edge, each naming the edge in its `message` (e.g. "Bleed margin empty on left edge"). A page with bleed on three sides and none on the fourth gets exactly one finding, for the deficient edge — an aggregate whole-perimeter check would let the other three sides mask it.
- **bbox:** that edge's probed strip rectangle in page user space (not the whole perimeter).
- **Fixup:** contributes `needs-auto-bleed` (once per page, regardless of how many edges fired — see below).

### `needs-auto-bleed`
- **Severity:** `info` (advisory; drives a fixup rather than failing the run).
- **Tier:** derived — emitted whenever `content-bleed` or `bleed-margin-empty` fired for a page.
- **When:** the document is a candidate for the `add-bleed` fixup (MIC-121/122/141).
- **bbox:** the page media/trim box (page-level advisory).
- **Fixup:** the presence of this finding is what makes `PreflightEngine` list `add-bleed` in `fixups_available` **dynamically** (see below).

### bbox coordinate convention — LOCK (verified, no value change)

Re-checked against `PDFDocumentDataLoaderDecorator::readRectangle` (`pdfdocument.cpp:245–277`): page boxes are built as `QRectF(xMin, yMin, xMax - xMin, yMax - yMin)` from the **raw PDF array values, unflipped** — `yMin`/`yMax` are the PDF box's bottom/top y exactly as read. So `rectToBbox()`'s `[left(), top(), right(), bottom()]` (`pdftoolpreflight.cpp`) already emits `[llx, lly, urx, ury]`, which **is** `[x0, y0, x1, y1]` in true PDF user space, media-box-lower-left origin (y-up) — the emitted values already match `report.schema.json` exactly. There is no output bug and no schema-value change needed.

**The real hazard is naming, not data:** `QRectF::top()`/`bottom()` conventionally imply Qt's y-down screen space, but this code holds y-up PDF values in them unflipped — `.top()` returns the PDF **bottom** edge, `.bottom()` returns the PDF **top** edge. That's a trap for new bbox-producing code, specifically the Tier-2 raster probe, which does genuine device-space (`QImage`) math that *is* y-down and must map back through `PDFRenderer::createPagePointToDevicePointMatrix()` — reusing `.top()/.bottom()` straight off a rendered strip's rect without that mapping would silently invert y.

**Lock:** no functional change to existing findings. Two documentation additions ride along with issue 4/5 implementation, worded here so the implementer copies them verbatim rather than re-deriving them:

- `report.schema.json` `$defs.bbox.description` gains one clause making the source explicit: *"...Origin is the page's media-box lower-left (PDF convention) — these are the raw PDF box array values, not a screen-space rectangle...."*
- A code comment lands above `rectToBbox()` in `pdftoolpreflight.cpp`:
  ```cpp
  // Page boxes are QRectF(llx, lly, width, height) — PDFDocumentDataLoaderDecorator::
  // readRectangle loads raw PDF array values unflipped, so QRectF::top()/bottom() here
  // hold PDF's bottom/top edges (y-up), NOT Qt's usual y-down screen convention. Do not
  // derive bbox from a rendered QImage's device-space rect without mapping back through
  // PDFRenderer::createPagePointToDevicePointMatrix() first.
  ```

New Tier-1/Tier-2 findings must produce `bbox`es via the same unflipped `QRectF` convention (page user space, not device space) so they stay consistent with existing `bleed`/`trim`/`page-size` findings and Phase-2 overlays.

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

**Tier-1 content bounds** come from the compiled page via `PDFPrecompiledPage::calculateGraphicPieceInfos(mediaBox, epsilon)` (returns `GraphicPieceInfo`s, each with a `boundingRect` and a `Type` of Text/VectorGraphics/Image/Shading, in page user space). Union the `boundingRect`s into the content-bounds rectangle and feed it to `contentWithinBleed`. This is the concrete API MIC-158 builds on — the per-piece `Type` also lets a later refinement ignore, say, registration marks if needed.

**`PDFBleedMarginProbe` (MIC-155)** takes a `PDFDocumentSession&`, a page index, and probes **each of the four edges independently** (top/bottom/left/right strip rectangles, each `amount_pt` deep along the trim/bleed boundary on that side), rasterizing every strip via `PDFRenderer::render(QPainter*, const QTransform&, pageIndex)` with `ClipToCropBox` **off** — the strip pattern already implemented in `pdfbleedfixup.cpp` (~585–661), which today rasterizes full-page then crops. Each edge gets its own coverage verdict; the probe never averages across the whole perimeter, so a single deficient edge cannot be masked by ink on the other three. Generalizing full-page-then-crop into a true sub-rect render (four strips instead of one) is the P3 stretch (MIC-159); the probe can start by cropping four strips from one full-page raster and swap to true sub-rect rendering later without changing its per-edge signature.

```cpp
struct PDFBleedMarginProbeResult
{
    std::array<bool, 4> edgeEmpty;      // indexed by Edge{Top,Bottom,Left,Right}
    std::array<QRectF, 4> edgeStrips;   // page user space, per-edge bbox for findings
};
PDFBleedMarginProbeResult probe(PDFDocumentSession& session, PDFInteger pageIndex,
                                const QRectF& trim, const QRectF& bleed, qreal amountPt,
                                int dpi = 150, qreal whiteThreshold = 0.9975);
```

## DPI / threshold defaults

Locked here (not in AGENTS.md) per `docs/PLANNING.md`. All reviewable in this M0.

**Naming reconciliation (found during review):** MIC-155 and MIC-160 already named three profile params — `raster_confirm`, `raster_confirm_dpi` (default **72**), `raster_white_threshold` (default **0.95**) — predating this doc. This M0 doc **adopts their three-param naming** (cleaner than a single implicit bool) but **supersedes both numeric defaults**, reasoned below. MIC-155/MIC-160 descriptions are updated to match as part of this revision.

| Param | Default | Notes |
|-------|---------|-------|
| `raster_confirm` | `false` (unset) | Per-check bool; gates whether Tier 2 runs at all. Unset ⇒ default profile behavior/cost unchanged. |
| `raster_confirm_dpi` | **150** (was 72) | 72 DPI makes a 9pt bleed strip only **~6px** deep (0.125in × 72) — too coarse to distinguish faint content from noise. 150 DPI gives **~19px**, still ¼ the pixels of a 300-DPI probe, but enough to compute a meaningful coverage ratio. |
| `raster_white_threshold` | **0.9975** (was 0.95) | Fraction of an edge strip that must be background/white before that edge is called empty. 0.95 (the old default) would call a strip "empty" even with **5% ink coverage** — large enough to be genuine bleed content, not noise. 0.9975 (⇔ ≤0.25% non-background) only fires on strips that are *actually* blank within antialiasing/compression noise (~0.01–0.05%), while still clearing faint-but-real bleed (light gradients, watermarks, thin rules) that a laxer 0.95 bar would already have passed anyway — so the practical gap between the two thresholds is at the *strict* end, not the lax end: 0.95 was simply too permissive to catch much of anything. |
| Coverage scope | **per-edge** (4 independent strips: top/bottom/left/right) | Not an aggregate over the whole perimeter (see below) — that gap in the original MIC-155/160 wording ("strip rects" / no scope stated) is closed here. |
| Bleed reach | `amount_pt` = **9** | Existing `frisket-default` value (9 pt ≈ 0.125 in). |
| Bleed/content tolerance | `tolerance_pt` = **0.25** when unset | Promotes today's hard-coded `isBleedAdequate` value to a profile param. |
| Render features | `ClipToCropBox` off, annotations off | Match `pdfbleedfixup.cpp` strip render; probe the full bleed margin, not the crop. |

**Why per-edge is the more consequential lock, independent of the exact threshold.** For a Letter page (8.5×11in) at 9pt bleed depth and 150 DPI, one edge strip is on the order of tens of thousands of pixels; a 0.25%-non-background bar is roughly a 0.11×0.11in patch of ink needed *on that edge* before the probe calls it "not empty." Genuinely blank margins sit at a noise floor around 0.01–0.05%, well under that bar either way — so the exact percentage mostly matters for faint-but-real bleed, not for catching truly blank margins. Switching from one aggregate perimeter number to **four independent per-edge checks** matters more than the percentage: an aggregate metric lets solid bleed on three sides carry a genuinely blank fourth side over the bar, silently missing exactly the defect this check exists to catch.

Profile example (Sinalite-style, opts into Tier 2):

```json
{
  "id": "bleed",
  "required": true,
  "amount_pt": 9,
  "tolerance_pt": 0.25,
  "raster_confirm": true,
  "raster_confirm_dpi": 150,
  "raster_white_threshold": 0.9975,
  "severity": "error"
}
```

All three `raster_*` params fit `profile.schema.json`'s check objects (which allow `additionalProperties: true`); register them in the schema and the README check-params table alongside `amount_pt` / `min_dpi` / `allowed`.

## Editor / canvas reuse notes (MIC-137)

`PDFDocumentSession` is designed so the Editor can later delegate to one Core session instead of the widget-bound split it uses today:

- The Editor's de-facto session is spread across `PDFDrawSpaceController` (owns `PDFFontCache`, holds document + `PDFOptionalContentActivity`) and `PDFDrawWidgetProxy` (owns the `PDFAsynchronousPageCompiler` with its `QCache<PDFInteger, PDFPrecompiledPage>`, exposes `getCMSManager`).
- `PDFDocumentSession` mirrors that getter surface (`document()`, `fontCache()`, `cms()`, `compiledPage()`) so `PDFDrawWidgetProxy` could hold or delegate to a session without changing its callers.
- The Frisket Preflight plugin (MIC-137, done) currently only displays report JSON produced by the CLI. Once the session exists, an in-process preflight run in the Editor could share the already-compiled pages of the open document instead of recompiling — the payoff of putting the cache in Core.
- **Actual Editor wiring is the P2 stretch (MIC-156 / issue 8), not this epic's core.** This doc only locks the session's shape so that reuse stays possible.

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
