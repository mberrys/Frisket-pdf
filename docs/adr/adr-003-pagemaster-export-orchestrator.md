# ADR-003: PageMaster export orchestrator



**Status:** accepted

**Date:** 2026-07-20

**Deciders:** MIC-311 / Cycle 2 sprint plan

**Amended:** 2026-07-21 (MIC-307 one-output retention + combined progress)



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

  `PDFDocumentWriter::write`. Stages run **per output**, not as batch-wide

  assemble-all / optimize-all / write-all passes.

- **Retention (MIC-307):** at most one live assembled `PDFDocument` at a time.

  After each successful write (or when that output fails), the document leaves

  scope — no cross-output `assembledDocumentStorage` vector.

- **Progress (MIC-307):** a single combined phase —

  `PDFProgress::start(N)` with text `"Exporting documents..."`, one `step()`

  after each completed output, then `finish()`. Image optimize is called with

  `progress = nullptr` so it does not nest a second progress phase.

- **Partial-batch failure:** outputs already written remain on disk and stay

  listed in `PDFPageMasterExportResult::writtenFiles` (needed for MIC-309).

- **Thread-safety:** synchronous, not thread-safe. PageMaster may call `run()`

  from a worker thread via `QtConcurrent`; a single run at a time.

- **Drivers stay thin:** PageMaster `MainWindow` builds the job (filenames,

  dialogs, bleed confirm) and wires progress UI. Filename templating and

  workspace JSON remain in PageMaster.

- **Follow-ons on this service:** MIC-308 (cancel token), MIC-309 (atomic write

  + manifest), MIC-312 (batch preflight gate).



## Consequences



- `MainWindow` deletes local `ExportJob` / `runExportJob` / `ExportResult`.

- UnitTests can exercise pipeline order and failure paths in-process without

  launching the PageMaster GUI.

- Later cycle-2 issues amend this class instead of forking a second pipeline.

- Peak assembled-document memory is bounded independently of batch size.

