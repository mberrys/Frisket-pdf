# Sprint Plan — Cycle 2 (Jul 20 – Aug 3, 2026): PageMaster Export Hardening

Status: **mostly complete** (refresh 2026-07-24). Batch stack MIC-307–312 and MIC-319 **Done**. Remaining V1 focus: **MIC-301** (In Review). **MIC-320** ships as a documented limitation with in-app disclosure; **MIC-336** is retargeted post-V1 (macOS is not a V1 platform). OCR must not displace those gates.

## Outcome table

| Issue | Role | Status |
|-------|------|--------|
| MIC-311 | Headless PageMaster export service + tests | Done |
| MIC-308 | Cooperative cancel + non-blocking close | Done |
| MIC-307 | One-output-at-a-time memory bound | Done |
| MIC-309 | Atomic writes + manifest + resume | Done |
| MIC-312 | Batch preflight gate + per-output reports | Done |
| MIC-319 | White-overprint detection findings | Done |
| MIC-320 | Overprint-correct rendering | Todo (V1 gate) |
| MIC-301 | Windows installer / clean-machine | In Review (V1 gate) |
| MIC-336 | macOS + packaging compatibility pass | Retargeted post-V1 (not a V1 gate) |

Original committed plan retained below for history.

relations on the issues.

## Context

Cycle 1 delivered Frisket V1 (operator loop, MIC-300) and stabilized CI. The four
High-priority Frisket tickets filed from the Dev Issue Tracker (MIC-306–309) all target
PageMaster export robustness. The export job in `Pdf4QtPageMaster/mainwindow.cpp:464-584`
assembles all outputs into `assembledDocumentStorage` before optimizing and writing any
(unbounded peak memory), has no cancellation contract (`~MainWindow` blocks on
`m_exportWatcher->waitForFinished()`), and tracks written files only in memory, so a
mid-batch failure leaves partial output with no manifest or resume state.

## Sprint goal

A PageMaster batch export that is memory-bounded, cancelable, and crash-safe — with the
export logic extracted into a headless, unit-testable service.

## Committed scope and sequencing

MIC-307/308/309 all rewrite the same export loop, so the refactor enabler goes first and
the three behaviors land as increments on the new service.

### Week 1 (Jul 21–26)

1. **MIC-311 — Extract a headless PageMaster export service** (enabler, pulled forward)
   - Move the assemble → geometry/bleed fixup → optimize → write pipeline out of
     `MainWindow`, following the PreflightEngine / ADR-002 orchestrator pattern.
   - Short ADR in `docs/adr/` first (M0 before code, per `docs/PLANNING.md`).
   - Pipeline-order and failure-path integration tests in `UnitTests/`
     (style of `tst_operatoracceptance.cpp`).
2. **MIC-308 — Cooperative cancellation + non-blocking close** (M, bug)
   - Cancellation token through the service loop; replace the destructor's
     `waitForFinished()` with request-cancel + bounded wait.
3. **MIC-307 — One-output-at-a-time processing** (L)
   - Assemble → optimize → write each output before starting the next; eliminate
     `assembledDocumentStorage` retention.

### Week 2 (Jul 27 – Aug 3)

4. **MIC-309 — Atomic per-output writes + partial-result manifest + resume** (L, bug)
   - Temp file + rename per output; persisted batch manifest
     (written / failed / pending) enabling resume or cleanup.
5. **MIC-306 — Overprint** (XL, split into sub-issues)
   - **MIC-319** (this cycle): detect unsafe/white overprint as preflight findings.
   - **MIC-320** (expected spillover to Cycle 3): overprint-correct rendering in
     standard and advanced renderers
     (`Pdf4QtLibCore/sources/pdftransparencyrenderer.cpp:584-644, 2630-2641`).

### Stretch (only if committed scope is done)

- **MIC-312 — PageMaster batch-preflight gate with per-output reports** — builds
  directly on the MIC-311 service.

## Verification

- New ctest targets pass locally and in `ci.yml` (Ubuntu + Windows).
- MIC-307: peak RSS stays flat as output count grows on a large multi-output batch.
- MIC-308: closing the window mid-batch exits promptly; no untracked partial output.
- MIC-309: killing the process mid-batch leaves no torn PDFs; manifest identifies
  completed vs. pending outputs on restart.
- MIC-319: golden-corpus white-overprint fixture produces the new finding; snapshot
  added under `frisket-preflight/testdata/`.
