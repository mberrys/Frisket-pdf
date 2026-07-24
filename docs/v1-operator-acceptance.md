# V1 operator acceptance (MIC-300)

Phase C acceptance for the first sellable Frisket operator loop: open a PDF, run the default profile, inspect findings, navigate visual regions, apply a confirmed bleed fix to a new file, and re-run preflight.

Automated coverage lives in `UnitTests/tst_operatoracceptance.cpp` (`ctest -R UnitTestsOperatorAcceptance`). The manual checklist below covers Editor UI behavior that headless PdfTool tests cannot exercise.

## Representative corpus

| Role | Fixture | Expected preflight |
|------|---------|-------------------|
| Clean pass | `bleed-adequate.pdf` | Pass |
| Missing / short bleed | `bleed-missing.pdf` | Fail (`bleed`), `add-bleed` advertised |
| Live text (embedded font) | `font-embedded.pdf` | Pass |
| Live text (not embedded) | `font-not-embedded.pdf` | Fail (`embedded-fonts`) |
| Image-only raster | `image-dpi-ok.pdf` | Pass |
| Malformed / unsupported | `malformed-not-pdf.pdf` | Non-zero exit (not findings exit code 1); no `%PDF` header |

Additional stress fixtures (`ai-art-*.pdf`) are exercised by `UnitTestsBleedStress`.

## Automated checks (`UnitTestsOperatorAcceptance`)

- Runs `PdfTool preflight` on each corpus file with `profiles/frisket-default.json`
- Validates normalized report contract (`preflightsidecarutils.h`)
- Full operator loop on `bleed-missing.pdf`: preflight → `add-bleed` (params from `fixups_available`) → re-preflight pass with plugin report validation
- Verifies the source PDF SHA-256 is unchanged after fixup (save-as semantics)
- Unicode + space paths for preflight input and `add-bleed` output (`shop files/café poster.pdf`)
- Invalid profile JSON returns actionable stderr (not exit code 1)
- Valid JSON profile with semantic mismatch (empty `checks`) returns exit code 1 with `type: profile` finding accepted by the plugin contract
- Unsupported `schema_version` and invalid scope combinations are rejected with explicit errors
- Visual vs non-visual finding classification (`bleed` page bbox vs `embedded-fonts` object without bbox)
- Sidecar cancellation: start preflight, wait for I/O, kill process, verify non-success termination
- Logs wall-time baseline for the corpus; on Linux also samples peak PdfTool child `VmHWM` (informational; not a perf gate)

## Manual operator checklist (Editor)

Run on a supported Windows or Linux machine with a release or dev build that bundles `PdfTool`, `FrisketPreflightPlugin`, and `share/frisket/profiles/frisket-default.json` (see [PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md)).

### Open and preflight

1. Launch **Pdf4QtEditor** and open `frisket-preflight/testdata/fixtures/bleed-missing.pdf`.
2. **Frisket Preflight → Run Preflight**.
3. Confirm the report panel opens with profile name, pass/fail summary, and at least one `bleed` error.
4. Confirm `add-bleed` appears under advertised fixups.

### Findings inspection

5. Select a **page-scoped bleed** finding → verify the viewer navigates to page 1 and draws a red/orange bbox overlay aligned with the page.
6. Open `font-not-embedded.pdf`, run preflight, select the **embedded-fonts** finding → verify details appear in the table and **no** fake page rectangle is drawn.
7. Zoom and rotate the page → overlays stay aligned with page content.

### Bleed fixup (save-as)

8. On `bleed-missing.pdf`, click **Apply Bleed Fix...**
9. Choose **Mirror**, 3.175 mm (9 pt), and an output path such as `C:\temp\venue_bleed.pdf`.
10. Confirm success message; verify the new file exists and the **open document is unchanged** (re-open original from disk if needed).
11. With **Re-run preflight after fixing** checked, confirm the panel shows post-fix results and bleed errors are cleared.

### Error handling

12. **Cancel mid-preflight** (close document or switch files while running) → UI stays responsive; no orphan `PdfTool` process (Task Manager).
13. Point preflight at a path with spaces and non-ASCII characters (e.g. copy a fixture to `shop files\café poster.pdf`) → preflight completes.
14. Open `malformed-not-pdf.pdf` → preflight shows an actionable error dialog (not a silent hang).

### Artifacts

15. Note report JSON on stdout when using `PdfTool preflight` from a terminal; Editor shows the same contract in the dock widget.
16. Optional: compare automated baseline timing from `corpus_performanceBaseline` test output after `ctest -R UnitTestsOperatorAcceptance`.

## Known limitations (V1)

- Only **add-bleed** is implemented in the plugin; other advertised fixups are filtered out.
- Post-fix preflight uses the bundled default profile, not a custom profile path.
- Mirror bleed can show seams on high-contrast corners (see `docs/bleed-stress-test-results.md`).
- **Overprint is not simulated in standard page rendering** (MIC-320). Overprint-accurate compositing exists only in the transparency renderer behind **Output Preview**. Preflight still *detects* white/near-white overprint and the report panel says so, but the page view will not show it. Do not use the page view to proof overprint-bearing prepress work.
- Malformed PDF handling depends on the core reader; some corrupt files may parse partially before failing a check.
- OCR, Notion, database, and network services are not required and are out of scope for this acceptance pass.
- Redaction verification (`MIC-305`) is a separate workflow; not part of this operator loop.

## Running automated acceptance

```powershell
# After building PdfTool + UnitTestsOperatorAcceptance
ctest -R UnitTestsOperatorAcceptance --output-on-failure
```

On Linux CI (offscreen Qt):

```bash
QT_QPA_PLATFORM=offscreen ctest -R UnitTestsOperatorAcceptance --output-on-failure
```
