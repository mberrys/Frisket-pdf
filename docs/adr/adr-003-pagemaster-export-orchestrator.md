# ADR-003: PageMaster export orchestrator

**Status:** accepted
**Date:** 2026-07-20
**Deciders:** MIC-311 / Cycle 2 sprint plan

## Context

PageMaster batch export lives in `Pdf4QtPageMaster/mainwindow.cpp` as an anonymous
`runExportJob` helper. MIC-307/308/309 all rewrite that loop (memory bounding,
cancellation, atomic writes + manifest). Without a headless Core service, those
changes stay UI-coupled and untestable via `UnitTests`/`ctest`.

## Decision

- **Location:** `Pdf4QtLibCore/sources/pdfpagemasterexport.h/.cpp`, namespace `pdf`.
- **API:** `PDFPageMasterExport::run(PDFPageMasterExportJob)` returns
  `PDFPageMasterExportResult`. Job owns source `PDFDocument` copies and `QImage`
  sources; optional `PDFProgress*` is borrowed.
- **Stage order (locked):** assemble → `PDFPageGeometry::apply` (optional) →
  `PDFBleedFixup::apply` (optional) → `PDFImageOptimizer::optimize` (optional) →
  `PDFDocumentWriter::write`.
- **Retention (MIC-311):** all assembled documents are retained in memory until
  optimize/write (current MainWindow behavior). MIC-307 will switch to
  one-output-at-a-time processing on this same service.
- **Cancellation (MIC-308):** optional borrowed `std::atomic_bool* cancelFlag` on
  the job. `run()` polls it cooperatively at orchestrator stage boundaries
  (before each assemble / geometry / bleed / optimize / write item). Mid-stage
  work inside geometry/bleed/optimize/write is not interruptible here.
  Optional borrowed `std::atomic_bool* progressAlive` (default treat as alive):
  when false, progress callbacks are skipped so a detaching UI can destroy its
  `PDFProgress` safely. On cancel: `success = false`, `cancelled = true`,
  `writtenFiles` lists only fully completed writes (never an in-flight path).
  PageMaster close/destructor uses request-cancel + invalidate-progress +
  bounded wait (3000 ms event-loop wait) then detaches the future watcher.
- **Thread-safety:** synchronous, not thread-safe. PageMaster may call `run()`
  from a worker thread via `QtConcurrent`; a single run at a time.
- **Drivers stay thin:** PageMaster `MainWindow` builds the job (filenames,
  dialogs, bleed confirm) and wires progress UI. Filename templating and
  workspace JSON remain in PageMaster.
- **Follow-ons on this service:** MIC-307 (per-output retention), MIC-309
  (atomic write + manifest), MIC-312 (batch preflight gate).

## Consequences

- `MainWindow` deletes local `ExportJob` / `runExportJob` / `ExportResult`.
- UnitTests can exercise pipeline order and failure paths in-process without
  launching the PageMaster GUI.
- Later cycle-2 issues amend this class instead of forking a second pipeline.
