# Frisket PDF — production runbook (V1)

Operational guide for shipping, monitoring, and supporting **Frisket PDF 1.6.x** desktop releases. This is not a hosted service runbook — there is no production API or database.

---

## 1. Release artifacts

| Platform | Artifact | Workflow | Install location |
|----------|----------|----------|------------------|
| Windows | MSI (signed optional) | `WindowsInstall.yml` | `C:\Program Files\Frisket PDF\` (WiX) |
| Windows | Portable zip | `ci.yml` | User-chosen |
| Linux | `.deb` | `ci.yml` → `make-package.sh` | `/usr/bin`, `/usr/lib/pdf4qt` |
| Linux | AppImage | `LinuxInstall.yml` | User-chosen |
| Linux | Flatpak | `LinuxFlatpak.yml` | Flathub-style bundle |

**V1 slim bundle** (`PDF4QT_FRISKET_DISTRIBUTION=ON`): Editor + PdfTool + core plugins only.

**Bundled paths (Linux):**

- Binaries: `usr/bin/Pdf4QtEditor`, `usr/bin/PdfTool`
- Plugins: `usr/lib/pdf4qt/`
- Preflight profile: `usr/share/frisket/profiles/frisket-default.json`

---

## 2. Deploy procedure

### 2.1 CI validation (required before release)

1. Confirm `ci.yml` is green on the release commit (`master` or release branch).
2. Verify jobs: **build_ubuntu**, **build_windows**, tests including:
   - `UnitTests`
   - `UnitTestsOperatorAcceptance`
   - `UnitTestsPreflightCorpus`
   - `UnitTestsPageMasterExport`
3. For release branches, confirm preflight corpus gate did not regress.

### 2.2 Windows MSI

```bash
# Manual dispatch: .github/workflows/WindowsInstall.yml
# Optional: SIGN_MSI=true with DigiCert Keylocker secrets configured
```

**Post-build smoke test (MIC-301):**

1. Clean Windows 10/11 VM (no Qt/MSVC installed).
2. Install MSI.
3. Launch **Pdf4QtEditor**.
4. Open `frisket-preflight/testdata/fixtures/bleed-missing.pdf`.
5. **Frisket Preflight → Run Preflight** — expect fail + bleed finding.
6. Confirm `PdfTool.exe` exists beside Editor and `share/frisket/profiles/frisket-default.json` is present.

### 2.3 Linux packages

- **`.deb`:** Install on Ubuntu 22.04+ VM; repeat smoke test above.
- **AppImage:** `chmod +x` and run; verify Fuse if needed.
- **Flatpak:** Note `--filesystem=host` — document for security-conscious users.

### 2.4 Draft GitHub release

```bash
# Manual dispatch: CreateReleaseDraft.yml
# Attaches latest AppImage + MSI artifacts to draft release tag v1.6.0.0
```

Publish draft only after smoke tests pass.

---

## 3. Rollback procedure

| Scenario | Action |
|----------|--------|
| Bad MSI/AppImage shipped | Unpublish GitHub release; re-point download to previous artifact |
| Regression in Editor only | Ship hotfix build; users reinstall |
| Bad preflight profile | Replace `frisket-default.json` in `share/frisket/profiles/` and rebuild; profiles are not auto-updated in place |
| PdfTool CLI break | Same bundle rollback — Editor plugin shells bundled PdfTool |

**No database migrations or feature flags** — rollback is binary replacement.

**User data:** Settings live in `%APPDATA%/MelkaJ/` (Windows) or `~/.config/MelkaJ/` (Linux). Rollback does not erase settings.

---

## 4. Monitoring and observability

### 4.1 CI health

- Watch: https://github.com/mberrys/Frisket-pdf/actions
- Alert on: `ci.yml` failure on `master`, preflight corpus failure, Windows build break.

### 4.2 Crash telemetry (optional, opt-in)

| Variable | Purpose |
|----------|---------|
| `SENTRY_DSN` | Enable crash reporting (runtime) |
| `SENTRY_ENVIRONMENT` | `production`, `staging`, etc. |
| `SENTRY_TRACES_SAMPLE_RATE` | 0.0–1.0 (default 0.2) |
| `SENTRY_DEBUG` | Verbose sentry-native logs (dev only) |

**Privacy:** Desktop sentry-native 0.15.x does not send default PII (`send_default_pii` is NX-only in that pin). Crashes may still include OS-level paths in minidumps — do not enable Sentry in high-classification environments without review.

**Verify:**

```bash
PdfTool sentry-verify   # requires SENTRY_DSN
```

### 4.3 Success metrics (manual / support tracking)

| Workflow | Success signal | Failure signal |
|----------|----------------|----------------|
| Preflight | Exit 0 or 1 + valid JSON report | Exit 2+, invalid JSON, hang |
| Add-bleed | Output PDF exists; re-preflight pass | `PDFOperationResult` error, zero-byte file |
| Editor open | Document renders | Password loop, crash |
| PageMaster batch | Manifest all `written` | `failed` entries, torn PDFs |
| OCR (optional) | Exit 0, report JSON | Exit 4 cancelled, sidecar missing |

### 4.4 Logs

- **PdfTool:** stderr (human-readable errors); stdout for JSON reports.
- **Editor:** Qt default; no log aggregation built-in.
- **Support:** Ask users to run failing command in terminal and attach stderr (redact file paths if needed).

---

## 5. Investigating failures

### 5.1 Preflight fails to start

| Symptom | Check |
|---------|-------|
| "Could not find PdfTool" | `PdfTool` beside `Pdf4QtEditor` in install prefix |
| "Could not find profile" | `share/frisket/profiles/frisket-default.json` relative to bin |
| Hang | Task Manager for stuck `PdfTool`; kill and retry |
| Invalid report JSON | stderr buffer overflow (>16 MB stdout) — reduce document complexity |

### 5.2 Preflight wrong results

1. Confirm profile: bundled `frisket-default.json` version.
2. Run headless: `PdfTool preflight <file> --profile <path> --console-format json`
3. Compare with `UnitTestsPreflightCorpus` fixtures.

### 5.3 Bleed fixup issues

```bash
PdfTool add-bleed input.pdf output.pdf --amount-mm 3.175 --mode mirror --force
PdfTool preflight output.pdf --profile profiles/frisket-default.json
```

Known limitation: mirror seams on high-contrast corners (`docs/bleed-stress-test-results.md`).

### 5.4 PageMaster batch export

1. Open batch manifest JSON (alongside outputs).
2. Status per output: `pending` | `written` | `failed`.
3. **Resume:** re-run export with resume enabled — skips `written` outputs.
4. If manifest says `pending` but PDF exists: may indicate interrupted run before manifest fix — delete orphan PDF or mark failed manually.

### 5.5 Crashes on specific PDFs

1. Check if encrypted — supply password.
2. Try `PdfTool info <file>` for parse errors.
3. File private security report per `SECURITY.md` with minimal reproducer.
4. Fuzz corpus in `Fuzz/` for similar patterns.

### 5.6 Attachment / security concerns

- Attachment extraction uses `PDFFilenameSanitizer` — see `docs/attachment-path-audit.md`.
- **Launch embedded files** is **off** by default (Settings → Security).
- **Open URI** is **off** by default; only http/https/mailto allowed when enabled.

---

## 6. Problematic uploads / untrusted PDFs

Frisket is a **local document processor**, not an upload service. Treat all PDFs as untrusted input.

| Risk | Mitigation |
|------|------------|
| Parser exploit | Fuzz CI; report via private advisory |
| Zip bomb / huge streams | OS memory limits; compiled page cache cap in settings |
| Path traversal via attachments | Sanitizer + containment (audited) |
| Embedded malware launch | Launch disabled by default + extension warning |
| JavaScript / URI exfiltration | URI launch disabled by default; no auto-network from PDF |

**User guidance for support:**

- Do not enable "Launch applications" unless required.
- Use Save As / export for outputs; originals are never modified by preflight (snapshot semantics).
- For sanitize/redact workflows, use dedicated tools and verify output.

---

## 7. Environment variables reference

| Variable | Component | Purpose |
|----------|-----------|---------|
| `SENTRY_DSN` | All GUI apps | Crash reporting |
| `FRISKET_OCR_SIDECAR` | PdfTool OCR | Path to OCR service executable |
| `QT_QPA_PLATFORM` | Headless CI | `offscreen` for tests |
| `PDF4QT_*` | Build-time | See `CMakeLists.txt` |

**Settings CLI:** `--config <path>` on all major apps for portable installs.

---

## 8. Escalation

| Level | Contact | When |
|-------|---------|------|
| L1 | GitHub Issues (non-security) | How-to, install problems |
| L2 | Maintainer | Reproducible bugs, release blockers |
| L3 | Private security advisory | Crashes on untrusted PDFs, path traversal, RCE | See `SECURITY.md` |

**Response targets (SECURITY.md):** acknowledge 7 days; fix/mitigate high-severity in 30 days.

---

## 9. Known limitations to expect in support

State these up front; each is a documented V1 behaviour, not a regression.

| Ref | Symptom a user will report | Answer |
|-----|----------------------------|--------|
| R-002 | "Overprint looks wrong in the page view" | Standard page rendering does not simulate overprint (MIC-320). Use **Output Preview** for overprint-accurate proofing. Preflight still flags white/near-white overprint and the report panel says so. Frisket V1 makes no overprint-safe output claim |
| R-016 | "Windows warns the installer is untrusted" | The V1 MSI is unsigned — no code-signing certificate is held yet. Expected SmartScreen behaviour; verify the download against `SHA256SUMS.txt` on the release |
| R-009 | "Changing the color scheme did nothing" | Settings are read at startup; restart the application |
| R-004 | "A `PdfTool` process is still running" | Only after the Editor is force-killed (Task Manager) mid-preflight. In-app cancel terminates the child cleanly. End the orphan manually |
| R-006 | "Why does the Flatpak want access to my whole home directory?" | The Flatpak grants `--filesystem=host` (MIC-328). Tightening to XDG portals is scheduled post-V1 |
| R-012 | "Mirror bleed shows seams" | Known limitation on high-contrast corner artwork — see `docs/bleed-stress-test-results.md` |
| — | "Is macOS supported?" | Not for V1. No macOS release assets are published; source builds are best-effort (`docs/PLATFORM_SUPPORT.md`) |

---

## 10. Pre-launch checklist (maintainer)

- [ ] **PR #54 merged** — the preflight schema contract (engine v3 / validator v2) is broken on `master` without it; the operator loop does not work
- [ ] `ci.yml` green on release SHA (`ci_ok` job passing)
- [ ] Branch protection requires the `ci_ok` status check on `master`
- [ ] MIC-301 clean-VM MSI smoke test completed via `scripts/Invoke-MsiSmokeTest.ps1`; transcript attached to the issue
- [ ] MSI install architecture confirmed 64-bit (see the `candle -arch x86` note in `docs/PLATFORM_SUPPORT.md`)
- [ ] MIC-320 documented as known limitation and disclosed in the report panel
- [ ] `PACKAGING_LICENSING.md` critical items reviewed (full checklist gates *paid* distribution)
- [ ] `THIRD_PARTY_NOTICES.txt` generated for the release artifact
- [ ] README points to Frisket release artifacts (not upstream PDF4QT)
- [ ] Draft release artifacts attached with `SHA256SUMS.txt`
- [ ] Release notes state: unsigned installer, no overprint simulation in page view, Windows + Linux only
- [ ] `docs/V1_RELEASE_READINESS.md` recommendation reviewed by product owner
