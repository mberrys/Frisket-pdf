# ADR-004: PageMaster batch manifest and atomic per-output writes

**Status:** accepted
**Date:** 2026-07-20
**Deciders:** MIC-309 / Cycle 2 sprint plan

## Context

`PDFPageMasterExport::run()` (ADR-003) writes outputs sequentially and lists
completed paths in `writtenFiles`, but a mid-batch failure or process kill can
leave torn PDFs on disk with no durable record of which outputs finished.
MIC-309 requires atomic per-output commits and a partial-result manifest that
enables resume or cleanup.

## Decision

- **Manifest file:** `.frisket-batch.json` in the directory of the first output
  path in the batch. One manifest per export invocation; `batch_id` is a UUID
  string generated at batch start.
- **Manifest schema (version 1):**

```json
{
  "schema_version": 1,
  "batch_id": "<uuid>",
  "outputs": [
    {
      "path": "<absolute or as-given output path>",
      "status": "pending|written|failed|skipped",
      "error": "<optional string when failed>"
    }
  ]
}
```

- **Atomic write:** each output is written to `<finalPath>.<pid>.partial` via
  `PDFDocumentWriter`, then renamed to `finalPath` with `QFile::rename` only
  after a successful write. Failed rename removes the partial file.
- **Manifest updates:** after each output attempt (success or failure), rewrite
  the manifest atomically (write `.frisket-batch.json.<pid>.partial`, rename).
- **Resume:** when `PDFPageMasterExportJob::resume` is true and a manifest
  exists beside the first output, outputs with `status: written` are skipped
  (path must still exist on disk). `pending` and `failed` are retried.
  `skipped` entries are never rewritten.
- **API additions:**
  - `PDFPageMasterExportJob::resume` (default `false`)
  - `PDFPageMasterExportJob::manifestPath` (optional override; default derived
    from first output directory)
  - `PDFPageMasterExportResult::manifestPath` and `QJsonObject manifest`
- **PageMaster UI:** auto-enable resume when an existing manifest is detected
  in the export directory (no separate wizard in this slice).

## Consequences

- Mid-batch kill leaves only complete outputs plus a manifest identifying state.
- Resume is headless-testable via `UnitTestsPageMasterExport`.
- MIC-312 batch preflight can key off the same manifest paths.
