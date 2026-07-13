# ADR-001: PDFDocumentSession lifetime, caching, and thread-safety

**Status:** accepted
**Date:** 2026-07-12
**Deciders:** MIC-151 epic review

## Context

MIC-151 introduces `PDFDocumentSession` as a shared Core session that owns a
`PDFDocument` plus compile-cache and decoded-stream cache. It must be reusable
from PdfTool (headless), Editor (GUI), and PageMaster (batch).

## Decision

- **Ownership:** `PDFDocumentSession` takes `PDFDocument*` (non-owning reference).
  The session does not delete the document; the caller owns the document lifetime.
- **Compile cache:** `std::unordered_map<size_t, PDFPrecompiledPage>` keyed by
  zero-based page index. Populated lazily via `compilePage()`. Immutable pages.
- **Decoded-stream cache:** `std::unordered_map<PDFObjectReference, QByteArray>`.
  Populated lazily via `getDecodedStream()`. Immutable decoded bytes.
- **Invalidation:** Call `invalidate()` to clear all caches. Typically called
  when the underlying `PDFModifiedDocument` fires a mutation signal.
- **Thread-safety:** Not thread-safe for writes. Const reads from the cache
  (after all writes complete) are safe. The session is single-threaded by
  design; multi-threaded page evaluation should use per-thread sessions.
- **Namespace:** `pdf::PDFDocumentSession` in `Pdf4QtLibCore`.

## Consequences

- Preflight checks that render the same page twice (e.g., Tier-1 bleed + Tier-2
  content-bleed for the same page) benefit from the compile cache.
- Editor and PageMaster can construct their own `PDFDocumentSession` from the
  same `PDFModifiedDocument` reference.
- Not thread-safe means parallel page preflight (if added later) must clone or
  synchronize; that is left for MIC-159 (generalized partial-page render).
