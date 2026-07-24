# V1 release readiness audit

Audit date: **2026-07-23**  
Product: **Frisket PDF 1.6.0.0** (Qt6 desktop PDF toolkit)  
Scope: operational, security, reliability, data-integrity, compatibility, and release-readiness for first public launch.

## Executive recommendation

**Ready with explicit risks.**

The V1 operator loop (Editor → preflight → findings → add-bleed → re-preflight) is automated in `UnitTestsOperatorAcceptance` and documented in `docs/v1-operator-acceptance.md`. CI builds Ubuntu + Windows, runs `ctest`, and gates on the preflight corpus. Attachment path traversal and batch export atomicity have been hardened.

**Blockers before marketing a general-audience V1:**

| ID | Risk | Mitigation required |
|----|------|---------------------|
| **R-001** | MIC-301 — Windows installer / clean-machine validation still In Review | Complete signed MSI smoke test on a VM without dev tools |
| **R-002** | MIC-320 — Overprint-correct rendering not implemented | Document as known limitation for print-shop CMYK/overprint workflows; do not claim overprint-safe output |

No web SaaS, accounts, or payments exist — many classic launch checklist items are **N/A** (see §Not applicable).

---

## 1. Release surface inventory

### User roles

| Role | Surface | Notes |
|------|---------|-------|
| **Operator** | Pdf4QtEditor + FrisketPreflightPlugin | Primary V1 sellable loop |
| **Automation / CI** | PdfTool CLI | `preflight`, `add-bleed`, `ocr` (optional) |
| **Power user** | PageMaster, Diff, Viewer, LaunchPad | Adjacent; not V1 contract |
| **Maintainer** | GitHub Actions, packaging scripts | Release engineering |

There are **no** tenant roles, admin consoles, or hosted user accounts.

### Environments and deployment targets

| Target | Mechanism | Path |
|--------|-----------|------|
| Linux CI | `.github/workflows/ci.yml` | Ubuntu build + `ctest` + `.deb` artifact |
| Windows CI | `.github/workflows/ci.yml` | Build + zip artifact |
| Windows MSI | `.github/workflows/WindowsInstall.yml` | `WixInstaller/` |
| Linux AppImage | `.github/workflows/LinuxInstall.yml` | Manual dispatch |
| Linux Flatpak | `.github/workflows/LinuxFlatpak.yml` | `Flatpak/io.github.mberrys.Frisket-pdf.json` |
| Draft release | `.github/workflows/CreateReleaseDraft.yml` | Aggregates AppImage + MSI |

**Not in CI:** macOS builds.

### External services (optional)

| Service | Required? | Opt-in mechanism |
|---------|-----------|-------------------|
| **Sentry** crash telemetry | No | `SENTRY_DSN` env; Windows default build flag `PDF4QT_ENABLE_SENTRY` |
| **OCR Python sidecar** | No | `FRISKET_OCR_SIDECAR` / bundled `FrisketOcrService` |
| **GitHub / Sponsor links** | No | `QDesktopServices::openUrl` from Help menu only |

No payment processors, identity providers, or document cloud APIs.

### Launch-critical dependencies

| Dependency | SPOF? | Fallback |
|------------|-------|----------|
| Qt 6.9 runtime | Yes | User must install/bundle Qt (installers do) |
| Pdf4QtLibCore PDF engine | Yes | None — core product |
| Bundled `PdfTool` + `frisket-default.json` | Yes for Editor preflight | Actionable error if missing from bundle |
| vcpkg third-party libs (OpenJPEG, zlib, …) | Build-time | Static link in release builds |
| Tesseract/EasyOCR (OCR only) | Yes for OCR feature | OCR disabled if sidecar missing |

### PDF data lifecycle

```
Open/import (local file) → in-memory PDFDocument → process (render, preflight, fixup)
  → export/save-as (atomic QSaveFile) → user filesystem
Temp: QTemporaryDir for preflight snapshots, OCR page PNGs, attachment extract
Deletion: user deletes files; temp dirs removed on scope exit; sanitize strips metadata/attachments on request
Retention: none server-side; Sentry may retain crash minidumps if enabled (no PDF content by design)
Logging: stderr/stdout for PdfTool; no centralized log shipping in product
```

---

## 2. V1 release-readiness checklist

| # | Area | Check | Status | Evidence |
|---|------|-------|--------|----------|
| A1 | V1 operator loop | Automated acceptance tests pass | **Pass** | `UnitTests/tst_operatoracceptance.cpp` |
| A2 | Preflight corpus | Golden corpus gate in CI | **Pass** | `UnitTestsPreflightCorpus`, `ci.yml` |
| A3 | Bleed fixup | Source PDF unchanged after fixup (save-as) | **Pass** | Operator acceptance SHA-256 test |
| A4 | Attachment paths | Sanitizer + containment on all write paths | **Pass** | `docs/attachment-path-audit.md`, unit tests |
| A5 | Launch actions | Default off; extension prompt | **Pass** | `m_allowLaunchApplications=false` default |
| A6 | URI actions | http/https/mailto allowlist; default off | **Pass** | `pdfprogramcontroller.cpp` |
| A7 | Preflight sidecar | Bounded stdout/stderr; process kill on cancel | **Pass** | `preflightsidecarutils.h` limits; plugin `cancelPreflightRun` |
| A8 | PageMaster export | Atomic writes + manifest + cancel | **Pass** | `tst_pagemasterexporttest.cpp` |
| A9 | Manifest/PDF consistency | Roll back output if manifest persist fails | **Pass** (this audit) | `pdfpagemasterexport.cpp` fix |
| A10 | Sentry privacy | No default PII | **Pass** (this audit) | Desktop sentry-native 0.15.x defaults to no PII; NX-only setter not used |
| A11 | CI build | Ubuntu + Windows compile + test | **Pass** | `.github/workflows/ci.yml` |
| A12 | Windows installer | Clean-machine install | **Fail / open** | MIC-301 In Review |
| A13 | Overprint rendering | Correct overprint compositing in standard page view | **Deferred — mitigated** | MIC-320 deferred post-V1; detection (MIC-319) ships, plus in-app disclosure in the report panel and a documented limitation |
| A14 | Packaging SBOM / license evidence | MIC-140 checklist complete | **Partial** | `docs/PACKAGING_LICENSING.md` unchecked items |
| A15 | macOS build | Supported platform | **N/A** | Not in CI |
| A16 | Authentication / payments | Secure flows | **N/A** | Offline desktop; PDF password only |
| A17 | CSP / CORS / cookies | Web security headers | **N/A** | No web app |
| A18 | Browser compatibility | Supported browsers | **N/A** | No embedded browser |
| A19 | OCR product gate | Required for V1 | **N/A** | Explicitly out of MIC-300 scope |
| A20 | Fuzz regression | Weekly fuzz CI | **Pass** | `.github/workflows/fuzz.yml` |

---

## 3. Launch-risk register

Sorted by severity. **Owner** defaults to release engineering unless noted.

### Blocker

| ID | Impact | Affected users | Reproduction | Root cause | Fix / mitigation | Verification | Owner |
|----|--------|----------------|--------------|------------|------------------|--------------|-------|
| **R-001** | Cannot ship Windows installer confidently | All Windows users | Fresh VM without MSVC/Qt; install MSI | Installer pipeline not fully signed off (MIC-301) | Complete MSI smoke test; code-sign if `SIGN_MSI` enabled | Install → launch Editor → run preflight on sample PDF | Release |
| **R-002** | Page view does not simulate overprint | Print/prepress shops proofing overprint work | Open an overprint fixture; compare page view against Output Preview | Overprint is implemented in `pdftransparencyrenderer.cpp` (Output Preview) but absent from the standard QPainter path in `pdfpainter.cpp`; that renderer is RGB and overprint is a subtractive CMYK model, so correct handling there is an XL change (MIC-320) | **Accepted for V1:** documented limitation + in-app disclosure — the preflight report panel now tells the operator to use Output Preview whenever a `white-overprint` finding is present. No "overprint-safe output" claim in marketing | Run preflight on `white-overprint-form.pdf`; confirm the panel note appears; proof via Output Preview | Product |

### High

| ID | Impact | Affected users | Reproduction | Root cause | Fix / mitigation | Verification | Owner |
|----|--------|----------------|--------------|------------|------------------|--------------|-------|
| **R-003** | Malicious PDF crash / DoS | Anyone opening untrusted PDFs | Crafted PDF via fuzz corpus | Parser/codec attack surface | Continue fuzz CI; private disclosure via `SECURITY.md` | Fuzz workflow green; no open critical CVEs | Security |
| **R-004** | Orphan `PdfTool` if Editor killed hard | Operators canceling preflight | Kill Editor from Task Manager during preflight | OS-level process termination | Document: use in-app cancel; plugin kills child on normal close | Manual checklist item 12 in v1-operator-acceptance | Support |
| **R-005** | Packaging license gaps (Qt LGPL evidence) | Legal/compliance | Audit installer contents | MIC-140 checklist incomplete | Complete `PACKAGING_LICENSING.md` gate before enterprise sales | Checklist sign-off | Legal/Release |
| **R-006** | Flatpak broad filesystem access | Linux Flatpak users | Install Flatpak; inspect permissions | `--filesystem=host` in manifest | Document risk; consider tightening to `home` post-V1 | Flatpak manifest review | Release |

### Medium

| ID | Impact | Affected users | Reproduction | Root cause | Fix / mitigation | Verification | Owner |
|----|--------|----------------|--------------|------------|------------------|--------------|-------|
| **R-007** | Resume batch after manifest failure | PageMaster power users | Disk full during manifest write | Was: PDF written, manifest stale | **Fixed:** remove PDF on manifest failure | `tst_pagemasterexporttest` (existing manifest tests) | Core |
| **R-008** | Sentry receives file paths in crashes | Opt-in telemetry users | Crash with `SENTRY_DSN` set | Crashpad minidumps may include paths | Document `SENTRY_DSN` opt-in; desktop SDK defaults omit PII | `PdfTool sentry-verify` | Release |
| **R-009** | Theme/scheme requires restart | All GUI users | Change color scheme in settings | Settings read only at startup | Document in release notes | Manual | UX |
| **R-010** | OCR sidecar supply chain | OCR users | Point `FRISKET_OCR_SIDECAR` at unknown binary | External Python/PyInstaller bundle | Ship only signed/bundled sidecar; document env var | OCR README | Release |
| **R-011** | README links upstream releases | New users | Read install section | Fork branding drift | Update README install URLs to Frisket releases | README review | Docs |

### Low

| ID | Impact | Notes |
|----|--------|-------|
| **R-012** | Mirror bleed seams on high-contrast art | Known V1 limitation (`docs/bleed-stress-test-results.md`) |
| **R-013** | Only `add-bleed` fixup in plugin UI | Other fixups filtered by design |
| **R-014** | No macOS CI | Best-effort community builds only |

---

## 4. Implemented changes (this audit)

| Change | File | Rationale |
|--------|------|-----------|
| Roll back written PDF when batch manifest persist fails | `pdfpagemasterexport.cpp` | Prevents resume/state inconsistency (R-007) |
| Disable Sentry default PII | `pdfsentry.cpp` / docs | Confirmed desktop 0.15.x has no PII setter (NX-only); default remains off (R-008) |
| Set preflight `QProcess` working directory to app bundle dir | `frisketpreflightplugin.cpp` | Predictable sidecar resolution |

Prior commits on `Pre-P3-sanitize` also addressed bug sanitization and visual polish (see PR #54).

---

## 5. Validation evidence

| Command | Expected | Local VM (2026-07-23) |
|---------|----------|------------------------|
| `cmake --build build --target UnitTestsOperatorAcceptance` | Build OK | **Blocked** — no Qt/vcpkg in cloud VM |
| `ctest -R UnitTestsOperatorAcceptance` | All pass | **Blocked** |
| `ctest -R UnitTestsPreflightCorpus` | All pass | **Blocked** |
| `ctest -R UnitTestsPageMasterExport` | All pass (includes manifest tests) | **Blocked** |
| GitHub `ci.yml` on `master` / PR | Green | **Run in CI** on push |

**Authoritative validation:** GitHub Actions CI on the PR branch.

---

## 6. Not applicable (web SaaS checklist items)

The following were evaluated and are **out of scope** for this desktop product:

- Server-side authentication, sessions, CSRF, CORS, CSP
- Multi-tenant isolation, RBAC, password reset flows
- Payment processing, subscriptions, webhooks
- Browser support matrix (no WebEngine)
- SSRF from server (no server)
- Database migrations / cloud backups
- Rate limiting (no API)
- robots.txt / sitemap / analytics consent banners

Local equivalents are covered above (PDF passwords, attachment sanitization, atomic writes, optional Sentry).

---

## 7. References

- `docs/v1-operator-acceptance.md` — MIC-300 operator loop
- `docs/SPRINT_CYCLE_2_PLAN.md` — MIC-301, MIC-320 gates
- `docs/PACKAGING_LICENSING.md` — MIC-140 bundle policy
- `docs/attachment-path-audit.md` — MIC-303
- `SECURITY.md` — disclosure policy
- `docs/PRODUCTION_RUNBOOK.md` — deploy, rollback, support
