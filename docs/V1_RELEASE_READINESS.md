# V1 release readiness audit

Audit date: **2026-07-23**, revised **2026-07-24**, corrected **2026-07-25**
Product: **Frisket PDF 1.6.0.0** (Qt6 desktop PDF toolkit)
Scope: operational, security, reliability, data-integrity, compatibility, and release-readiness for first public launch.
Platforms: **Windows and Linux** (macOS is not a V1 platform — see `docs/PLATFORM_SUPPORT.md`).

## Executive recommendation

**Not ready — three open gates, one of them a broken security control. The previously
reported launch-blocking defect (R-000) is resolved.**

The 2026-07-23 revision concluded "ready with explicit risks." The 2026-07-24 revision
concluded "not ready — one launch-blocking defect." **Both were wrong.** This revision
supersedes them.

### R-000 was already fixed when it was reported

The 2026-07-24 revision stated that the V1 operator loop did not work on `master` because
the engine emitted `schema_version: 3` while the plugin validator capped at `2`, and that
*"the fix exists on the `Pre-P3-sanitize` branch (PR #54) and is unmerged."*

That was not true at the time of writing, and is not true now:

- `Pdf4QtLibCore/sources/preflightengine.h:42` → `PREFLIGHT_REPORT_SCHEMA_VERSION = 3`
- `Pdf4QtEditorPlugins/FrisketPreflightPlugin/preflightsidecarutils.h:38` →
  `FRISKET_PREFLIGHT_SCHEMA_VERSION 3`, with `isSupportedSchemaVersion` accepting 1–3
- The fix is commit `c515bfa3`, merged to `master` via `ba428f1b` (PR #54) at
  2026-07-24 10:58 PDT
- The revision asserting it was unmerged is commit `d43a70ec` (PR #56) at 11:59 PDT —
  **one hour later**, with `c515bfa3` already in its own history
  (`git merge-base --is-ancestor c515bfa3 d43a70ec` succeeds)

R-000 is struck. Because checklist item A11 attributed all CI redness to R-000, current CI
status must be re-derived rather than inherited from this document.

### Launch gates

| ID | Risk | Status | Linear | Mitigation required |
|----|------|--------|--------|---------------------|
| ~~R-000~~ | ~~Preflight report schema contract broken on `master`~~ | **Struck 2026-07-25** | — | Not a defect. Engine and validator are both at schema 3 on `master` |
| **R-003** | Fuzz CI has not run since `fuzz.yml` broke | **Blocking** | [MIC-326](https://linear.app/mbx2/issue/MIC-326) | Duplicate top-level `permissions:` key made the workflow invalid; every run failed at 0s. Fixed in this change — **requires one green run as evidence** |
| **R-001** | MIC-301 — installer / clean-machine validation still In Review | **Open** | [MIC-301](https://linear.app/mbx2/issue/MIC-301), [MIC-327](https://linear.app/mbx2/issue/MIC-327) | Run `scripts/Invoke-MsiSmokeTest.ps1` on a clean VM against an MSI from `WindowsInstall.yml`. **Now also covers Linux** `.deb`/AppImage clean-machine smoke, which had no gate at all |
| **R-016** | V1 MSI ships unsigned; no certificate held | **Decided 2026-07-25 — ship unsigned** | [MIC-342](https://linear.app/mbx2/issue/MIC-342) disclosure · [MIC-345](https://linear.app/mbx2/issue/MIC-345) procurement | Was the only risk in this register with no Linear issue. Blocking on a 1–3 week procurement would have slipped Aug 8. **Remaining obligation:** disclose the SmartScreen prompt in README, release notes, and runbook, and point users at `SHA256SUMS.txt` for integrity |
| **R-002** | MIC-320 — overprint not simulated in standard page rendering | **Accepted — mitigation incomplete** | [MIC-330](https://linear.app/mbx2/issue/MIC-330) | Documented limitation + in-app disclosure. **The disclosure fires only on `white-overprint` findings while the limitation is general** — see §3. Deferral of MIC-320 is now recorded in Linear (project description + MIC-306), not only here |
| ~~R-015~~ | ~~macOS declared supported without CI, packaging or notarization~~ | **Resolved** | [MIC-336](https://linear.app/mbx2/issue/MIC-336) | macOS restated as post-V1; V1 ships Windows + Linux. MIC-336 reopened — it had been closed Done with its acceptance unmet |

Two audit items previously listed as gaps were already built and only needed correcting:
`scripts/smoke-test-install.ps1` (most of MIC-301's functional checks) and
`scripts/generate-third-party-notices.ps1`.

### Product decisions — 2026-07-25

Recorded in the Linear project description, which is the authority. Reproduced here so this
document cannot drift from it again.

| Question | Decision |
|----------|----------|
| Overprint-correct rendering a V1 gate? | **No** — deferred post-V1 (MIC-320). V1 ships detection + disclosure, no overprint-safe claim |
| Sign the Windows installer? | **No** — V1 ships unsigned; procurement in parallel (MIC-345) |
| Linux a V1 platform? | **Yes** — Windows and Linux both ship V1. macOS post-V1 |
| OCR in V1? | **CLI-only** — `PdfTool ocr` only; no `OcrPlugin`, no bundled sidecar service (MIC-343) |
| V1 free or paid? | **Free public release** — licensing checklist gates first paid distribution |

The CLI-only OCR decision has a packaging consequence worth stating plainly: `PdfTool ocr`
ships but is **inert without a sidecar**, and no sidecar is bundled. OCR in V1 is
bring-your-own and unsupported. This keeps the "no OCR service required" property in the
product brief true, and keeps a Python/PyInstaller bundle out of the V1 attack surface.

> **Authority note (added 2026-07-25).** This document does **not** set the V1 gate list.
> The Linear project description does. The 2026-07-24 revision reclassified MIC-320 from
> launch gate to "Accepted" while the project description, roadmap, product brief, and
> MIC-306 all still called it a hard gate — leaving two live, contradictory definitions of
> V1 for two days. Both now agree that MIC-320 is deferred. Any future change to a gate
> must amend the project description in the same change.

No web SaaS, accounts, or payments exist — many classic launch checklist items are **N/A** (see §Not applicable).

**Commercial status:** V1 is a **free public release**. The MIC-140 / MIC-329 licensing
checklist (artifact SBOM, Qt LGPL relink evidence, counsel sign-off) therefore gates the
first *paid* distribution, not this launch. See §5 and `docs/PACKAGING_LICENSING.md`.

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
| Qt 6.11.1 runtime | Yes | User must install/bundle Qt (installers do) |
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
Retention: none server-side; Sentry may retain crash minidumps if enabled. Minidumps are
  captured by crashpad and include thread stacks and referenced heap memory, so they CAN
  contain PDF content and file paths. Nothing in the SDK can scrub them — see R-008.
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
| A10 | Sentry privacy | No default PII | **Pass, scope corrected** | Desktop sentry-native 0.15.x defaults to no PII; NX-only setter not used. That covers SDK-attached identifiers **only** — crashpad minidumps can still contain PDF content and paths, and no SDK hook can scrub them. The former "no PDF content by design" claim was unenforced by any code; it is now stated as a disclosed property of opting in (R-008) |
| A0 | Preflight report contract | Engine schema version accepted by plugin validator | **Pass** (corrected 2026-07-25) | Both at 3: `preflightengine.h:42`, `preflightsidecarutils.h:38` (`isSupportedSchemaVersion` accepts 1–3). Fix `c515bfa3` merged in `ba428f1b` |
| A11 | CI build | Ubuntu + Windows compile + test | **Re-derive** | Previously "Fail — all downstream of A0." A0 is not a defect, so that attribution is void. Read current status from Actions on `master`; do not inherit this row |
| A12 | Installer | Clean-machine install (**Windows + Linux**) | **Fail / open** | MIC-301 In Review; harness at `scripts/Invoke-MsiSmokeTest.ps1`, clean-VM run outstanding. Linux `.deb`/AppImage smoke added to MIC-301 on 2026-07-25 — previously ungated despite shipping in `CreateReleaseDraft.yml` |
| A13 | Overprint rendering | Correct overprint compositing in standard page view | **Deferred — mitigation incomplete** | MIC-320 deferred post-V1; detection (MIC-319) ships. In-app disclosure exists but triggers only on `white-overprint` findings (`preflightreportdockwidget.cpp:172`) while the limitation covers all overprint — MIC-330. README limitation text still missing |
| A14 | Packaging SBOM / license evidence | MIC-140 checklist complete | **Partial — gates paid distribution, not V1** | Notices generator now resolves versions + license text; artifact SBOM and counsel sign-off outstanding |
| A15 | macOS build | Supported platform | **N/A** | Not a V1 platform; no CI, no package, no notarization |
| A21 | Bundle policy enforcement | No Ghostscript / JRE / Python in default bundle | **Pass** (this revision) | Enforced by `scripts/smoke-test-install.ps1`, wired into `WindowsInstall.yml` |
| A22 | Artifact checksums | SHA-256 published with release | **Pass** (this revision) | `CreateReleaseDraft.yml` emits `SHA256SUMS.txt` |
| A16 | Authentication / payments | Secure flows | **N/A** | Offline desktop; PDF password only |
| A17 | CSP / CORS / cookies | Web security headers | **N/A** | No web app |
| A18 | Browser compatibility | Supported browsers | **N/A** | No embedded browser |
| A19 | OCR product gate | Required for V1 | **N/A — CLI-only** (decided 2026-07-25) | OCR is merged to `master` but V1 ships **CLI-only**: `PdfTool ocr` present; `OcrPlugin.dll` and the bundled sidecar service excluded. `PdfTool ocr` is inert without a user-supplied sidecar. Enforcement (all package formats + a build/bundle drift assertion) — MIC-343 |
| A20 | Fuzz regression | Weekly fuzz CI | **Fail — fixed here, unproven** | `fuzz.yml` had two top-level `permissions:` keys (lines 3 and 18, from CodeQL autofix `31a4444d`), making the workflow invalid. Every run failed at 0s. Previously recorded as "Pass" while nothing ran. Duplicate removed in this change; **flip to Pass only after one green run** — MIC-326 |
| A23 | Manifest rollback coverage | Failure path is tested, not just implemented | **Fail** (this revision) | `pdfpagemasterexport.cpp:590-592` removes the PDF when manifest persist fails, but no test forces that failure. R-007's stated verification cites success-path tests only — MIC-335 |

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
| **R-003** | Malicious PDF crash / DoS | Anyone opening untrusted PDFs | Crafted PDF via fuzz corpus | Parser/codec attack surface | **Escalated to Blocking 2026-07-25:** fuzz CI was not running at all — `fuzz.yml` invalid since CodeQL autofix `31a4444d` added a second top-level `permissions:` key. Harnesses (MIC-304) were fine; the workflow that runs them was dead, and A20 reported "Pass" throughout. Duplicate key removed in this change | Fuzz workflow **green** (not merely present); no open critical CVEs — MIC-326 | Security |
| **R-004** | Orphan `PdfTool` if Editor killed hard | Operators canceling preflight | Kill Editor from Task Manager during preflight | OS-level process termination | Document: use in-app cancel; plugin kills child on normal close | Manual checklist item 12 in v1-operator-acceptance | Support |
| **R-005** | Packaging license gaps (Qt LGPL evidence) | Legal/compliance | Audit installer contents | MIC-140 checklist incomplete | Complete `PACKAGING_LICENSING.md` gate before enterprise sales | Checklist sign-off | Legal/Release |
| **R-006** | Flatpak broad filesystem access | Linux Flatpak users | Install Flatpak; inspect permissions | `--filesystem=host` in manifest | Document risk; consider tightening to `home` post-V1 | Flatpak manifest review | Release |

### Medium

| ID | Impact | Affected users | Reproduction | Root cause | Fix / mitigation | Verification | Owner |
|----|--------|----------------|--------------|------------|------------------|--------------|-------|
| **R-007** | Resume batch after manifest failure | PageMaster power users | Disk full during manifest write | Was: PDF written, manifest stale | **Fixed:** remove PDF on manifest failure (`Pdf4QtLibCore/sources/pdfpagemasterexport.cpp:590-592`) | **Untested.** The cited "existing manifest tests" (`manifest_persistedWithWrittenStatuses`, `resume_skipsAlreadyWrittenOutputs`, `resume_mismatchedManifestStartsFreshBatch`) are all success-path; nothing forces a manifest-persist failure. Test tracked in MIC-335 | Core |
| **R-008** | Sentry crash minidumps can contain PDF content and file paths | Opt-in telemetry users | Crash with `SENTRY_DSN` set | Crashpad captures thread stacks and referenced heap memory out-of-process. A crash in the parser or content processor therefore has document bytes live in the dump. `before_send` cannot filter this — it applies to events, not the minidump upload | **Disclosure, not enforcement.** `SENTRY_DSN` is unset by default and must stay unset when handling confidential documents. Do not restate "no PDF content by design" — nothing implements it | `PdfTool sentry-verify`; confirm `SENTRY_DSN` unset in shipped configs | Release |
| **R-009** | Theme/scheme requires restart | All GUI users | Change color scheme in settings | Settings read only at startup | Document in release notes | Manual | UX |
| **R-010** | OCR sidecar supply chain | OCR users | Point `FRISKET_OCR_SIDECAR` at unknown binary | External Python/PyInstaller bundle | Ship only signed/bundled sidecar; document env var | OCR README | Release |
| **R-011** | README links upstream releases | New users | Read install section | Fork branding drift | Update README install URLs to Frisket releases | README review | Docs |

### Low

| ID | Impact | Linear | Notes |
|----|--------|--------|-------|
| **R-012** | Mirror bleed seams on high-contrast art | MIC-339 (Done) | Known V1 limitation (`docs/bleed-stress-test-results.md`, `PRODUCTION_RUNBOOK.md:233`). Operators can switch to pixel-repeat/stretch (MIC-122) |
| **R-013** | Only `add-bleed` fixup in plugin UI | MIC-338 | Other fixups filtered by design. Not yet stated in user-facing docs |
| **R-014** | No macOS CI | MIC-336 | macOS is not a V1 platform; source builds are best-effort (`docs/PLATFORM_SUPPORT.md`). **ID note:** MIC-336 previously reused R-014 to mean "add macOS support, High" — the inverse of this row. R-IDs are now immutable; that override has been removed |
| **R-016** | V1 MSI ships unsigned | **MIC-342** | **Escalated to a launch gate 2026-07-25 — see §Executive recommendation.** No code-signing certificate held; Windows users see SmartScreen on first install. DigiCert procurement is a parallel track with 1–3 week lead time, which likely makes this the schedule-determining item for an Aug 8 target. Had no Linear issue until 2026-07-25 — the 2026-07-23 risk wave filed R-001…R-014 and skipped it |

> **Register hygiene (added 2026-07-25).** Risk IDs are immutable once assigned: a given
> R-number means one thing permanently. Every row above must carry its Linear issue ID, and
> every risk must have one. Both rules were broken — R-014 was reused with an inverted
> meaning, and R-016 was never filed.

---

## 4. Implemented changes (this audit)

### 2026-07-25 corrections

| Change | File | Rationale |
|--------|------|-----------|
| Remove duplicate top-level `permissions:` key | `.github/workflows/fuzz.yml` | Workflow was invalid; every fuzz run failed at 0s while A20 reported "Pass" (R-003) |
| Strike R-000; restate executive recommendation | this file | The reported blocker was already fixed on `master` before the revision that reported it |
| Correct A0, A11, A13, A19, A20; add A23 | this file | Several rows asserted verification that did not exist |
| Add Linear IDs and immutability rule to the register | this file | R-014 was reused with an inverted meaning; R-016 had no issue |

### 2026-07-24 changes

| Change | File | Rationale |
|--------|------|-----------|
| Roll back written PDF when batch manifest persist fails | `Pdf4QtLibCore/sources/pdfpagemasterexport.cpp` | Prevents resume/state inconsistency (R-007) — **untested, see A23** |
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
