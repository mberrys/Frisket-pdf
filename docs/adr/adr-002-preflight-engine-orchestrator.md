# ADR-002: PreflightEngine orchestrator boundary

**Status:** accepted
**Date:** 2026-07-12
**Deciders:** MIC-151 epic review

## Context

MIC-154 refactors the flat loop in `PdfTool/pdftoolpreflight.cpp` into a reusable
`pdf::PreflightEngine` in Core. The engine must be callable from PdfTool (headless
CLI) and eventually from Editor (in-process via plugin) and PageMaster (batch).

## Decision

- **Location:** `Pdf4QtLibCore/sources/preflightengine.h/.cpp`, namespace `pdf`.
- **Construction:** `PreflightEngine(PDFDocumentSession*)`. The engine borrows
  the session; the caller manages session lifetime.
- **API:** `QJsonObject run(const QJsonObject& profile)`. Returns a normalized
  report conforming to `frisket-preflight/schemas/report.schema.json`.
- **Check registry:** Checks are registered by string ID → callable. The engine
  includes the existing checks (`bleed`, `trim`, `page-size`) plus stubs for
  `color-mode`, `image-resolution`, `embedded-fonts` (pending MIC-148/149/150).
  `content-bleed` is added in MIC-158.
- **Phase order:** Tier-1 structural checks run first. Tier-2 content checks run
  only when enabled in the profile and Tier-1 passed. Dynamic `fixups_available`
  collection runs last.
- **Raster constraints:** The engine (and `PDFBleedMarginProbe`) use `PDFRasterizer`
  via `RendererEngine::QPainter`. On GUI platforms, rasterizer construction must
  happen in the main thread. PdfTool satisfies this; Editor plugin shells out via
  QProcess and is unaffected. In-process Editor/PageMaster integration (P2) must
  respect the main-thread constraint or use `PDFRasterizerPool`.
- **Thread-safety:** The engine is not thread-safe. A single run is synchronous.

## Consequences

- `PdfTool/pdftoolpreflight.cpp` becomes a thin driver: load profile →
  create session → create engine → run → write JSON.
- The existing preflight checks (`bleed`, `trim`, `page-size`) and their
  math in `pdftoolpreflightchecks.h` move into the engine but keep the same
  pure-math header.
- The Plugin's QProcess path is unchanged; no plugin-side threading changes needed.
