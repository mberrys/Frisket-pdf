# Attachment path security audit (MIC-303)

Audit date: 2026-07-19

## Scope

Paths where PDF-embedded attachment filenames or URIs are written to disk or opened via the OS.

## Findings

| Location | Operation | Guard | Status |
|----------|-----------|-------|--------|
| `PdfTool/pdftoolattachments.cpp` | Save attachments to directory | `PDFFilenameSanitizer::sanitize()` + `isPathContained()` | OK (existing) |
| `Pdf4QtLibGui/pdfsidebarwidget.cpp` — Save to File | User-chosen path via `QFileDialog`; default name sanitized | `PDFFilenameSanitizer::sanitize()` | OK (existing) |
| `Pdf4QtLibGui/pdfsidebarwidget.cpp` — Open Attachment | Write to temp dir, then `QDesktopServices::openUrl` | `sanitize()` + `isPathContained()` + launch setting gate | **Fixed** (was `QFileInfo::fileName()` only) |
| `Pdf4QtLibGui/pdfprogramcontroller.cpp` — Launch action | Execute platform file from PDF | `m_allowLaunchApplications` + dangerous-extension prompt | OK (existing, commit f0bb09cb) |
| `Pdf4QtLibGui/pdfprogramcontroller.cpp` — URI action | Open URL | `m_allowLaunchURI` + scheme allowlist (http/https/mailto) | OK (existing) |
| `Pdf4QtLibWidgets/sources/pdfitemmodels.cpp` | Display attachment names in tree | Display only; no filesystem write | N/A |
| `Pdf4QtLaunchPad/launchapplication.cpp` | Start bundled apps | Hard-coded internal paths | N/A (not PDF-sourced) |

## Tests

- `UnitTests/tst_filenamesanitizertest.cpp` — sanitizer and `isPathContained` coverage
- `UnitTests/tst_filenamesanitizertest.cpp::test_attachmentOpenPath_contained` — mirrors sidebar open path construction

## Conclusion

All user-reachable attachment extraction and open paths use `PDFFilenameSanitizer` and containment checks where files are written under a target directory. No additional unguarded paths were found.
