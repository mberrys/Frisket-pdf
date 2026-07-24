# Platform support (Windows, Linux)

Frisket PDF V1 ships on **two** desktop platforms: Windows and Linux. This
document is the source of truth for supported OS, install layouts, and the
**cross-platform compatibility pass** over modules that are already
feature-complete.

**Linear:** [MIC-336](https://linear.app/mbx2/issue/MIC-336) (R-014) — retargeted
as a **post-V1** epic; see [macOS (post-V1)](#macos-post-v1).
**Related:** [MIC-301](https://linear.app/mbx2/issue/MIC-301) (Windows MSI),
[MIC-328](https://linear.app/mbx2/issue/MIC-328) (Flatpak permissions),
[MIC-329](https://linear.app/mbx2/issue/MIC-329) (licensing checklist),
`docs/PACKAGING_LICENSING.md`

## Supported platforms

| Platform | Status | CI | Official packages |
|----------|--------|----|-------------------|
| **Windows** (x64) | Supported | `ci.yml` + `WindowsInstall.yml` | MSI (`WixInstaller/`), portable zip |
| **Linux** (x64) | Supported | `ci.yml` + `LinuxInstall.yml` / `LinuxFlatpak.yml` | `.deb`, AppImage, Flatpak |
| **macOS** | **Not supported for V1** — source builds best-effort | None | None |

Unsupported for V1: macOS, iOS, Android, other BSDs. Community builds elsewhere
are best-effort only and produce no official release artifacts.

> **Claim discipline.** Do not describe macOS as a supported platform in README,
> marketing copy, or release notes while this table says otherwise. The code is
> expected to compile on macOS, but "compiles" is not "supported": there is no CI
> job proving it, no signed package, and no notarization.

## Install and plugin layout

| Platform | Binaries | Plugins (`PDF4QT_PLUGINS_DIR`) | Preflight profiles |
|----------|----------|--------------------------------|--------------------|
| Windows | `<prefix>/bin` (beside MSI tree) | `<prefix>/pdfplugins` (relative `../pdfplugins`) | `share/frisket/profiles/` in bundle |
| Linux | `<prefix>/bin` | `<prefix>/lib/pdf4qt` | `/usr/share/frisket/profiles/` (deb) |

Editor must resolve **PdfTool** and **FrisketPreflightPlugin** without a
developer toolchain on PATH.

The profile directory is set by `FRISKET_PREFLIGHT_PROFILES_DIR` in
`Pdf4QtEditorPlugins/FrisketPreflightPlugin/CMakeLists.txt`, derived from
`PDF4QT_INSTALL_SHARE_DIR`. Note that `-DPDF4QT_INSTALL_TO_USR=ON` (used by the
Windows CI and MSI builds) prefixes that with `usr/`, so the shipped path is
`usr/share/frisket/profiles/`. Any smoke test must derive this from the install
prefix rather than assuming a fixed absolute location.

## V1 slim distribution

When `PDF4QT_FRISKET_DISTRIBUTION=ON`, prefer Editor + PdfTool + core plugins
(FrisketPreflight and required inspection plugins). PageMaster / Diff / Viewer /
LaunchPad may ship in full packages; still build them in CI on both supported OS.

## Cross-platform compatibility pass

Run this pass before marketing V1 on any platform. Goal: correct **dependency
bundling** and **installer packaging** for modules that are already complete.

### Module checklist

| Module / surface | Already complete? | Win | Linux | What to verify |
|------------------|-------------------|:--:|:-----:|----------------|
| Pdf4QtLibCore | Yes | ☐ | ☐ | Qt 6.9 + vcpkg build; codecs/fonts; no Widgets |
| Pdf4QtLibWidgets / Pdf4QtLibGui | Yes | ☐ | ☐ | Plugin relative path; settings paths |
| PdfTool (`preflight`, `add-bleed`, …) | Yes | ☐ | ☐ | Bundled next to Editor; working directory; offscreen CI |
| FrisketPreflightPlugin | Yes | ☐ | ☐ | Finds PdfTool + `frisket-default.json`; `.dll` / `.so` |
| Pdf4QtEditor | Yes | ☐ | ☐ | Clean-machine launch; plugins load; operator loop |
| Other Editor plugins | Yes | ☐ | ☐ | Present in intended bundle set; load without system Qt |
| Pdf4QtPageMaster export (MIC-307–312) | Yes | ☐ | ☐ | Atomic write + manifest; cancel; case-sensitive FS |
| Pdf4QtViewer / Diff / LaunchPad | Adjacent | ☐ | ☐ | Build in CI; optional in slim package |
| frisket-preflight profiles + schemas | Yes | ☐ | ☐ | Installed at documented path; schema version contract |
| UnitTests (operator, corpus, PageMaster) | Yes | ☐ | ☐ | `ctest` green on both CI runners |
| Windows MSI | In review (MIC-301) | ☐ | — | Clean VM smoke; redist; signing |
| Linux `.deb` / AppImage / Flatpak | Exists | — | ☐ | Smoke; Flatpak `--filesystem=host` documented |
| Sentry (optional) | Partial | ☐ | ☐ | Opt-in DSN only; DB path; no default PII |
| OCR sidecar (optional) | Not V1-gated | ☐ | ☐ | Bundled-only guidance; do not block platform gate |

### Bundling rules (all OS)

1. Ship Qt runtime and required Qt plugins (`platforms`, `imageformats`, …) inside the package — do not require a system Qt install.
2. Co-locate PdfTool, FrisketPreflightPlugin, and `frisket-default.json` per the layout table.
3. Keep the default bundle C++/Qt only (see `docs/PACKAGING_LICENSING.md`); scan for forbidden Ghostscript / JRE / Python payloads.
4. Codesign where the platform expects it (Authenticode on Windows).
5. Document upgrade, uninstall, and binary rollback (no cloud DB).

### Smoke test (every installer)

1. Clean machine (no Qt / MSVC required at runtime).
2. Install the platform package.
3. Launch Pdf4QtEditor.
4. Open a sample PDF; run Frisket Preflight; confirm findings JSON.
5. Confirm PdfTool exists beside the app and profiles resolve.

Windows automation for steps 2–5 lives in `scripts/smoke-test-install.ps1`.

## macOS (post-V1)

macOS is explicitly **out of scope for V1**. The work below is retained as the
entry criteria for adding it in a later release, not as a V1 checklist.

- Apps already set `MACOSX_BUNDLE ON` for Editor, Viewer, PageMaster, Diff, LaunchPad.
- CMake today treats non-`PDF4QT_LINUX` like Windows for `PDF4QT_PLUGINS_DIR` (`pdfplugins`, `CMakeLists.txt:198-201`). That path must be confirmed inside a `.app` bundle or the install rules adjusted.
- A `macos` job in `ci.yml` with Qt 6.9 + vcpkg, mirroring the Ubuntu/Windows `ctest` set, is the minimum bar before any macOS claim is restored.
- Notarization and staple steps belong in a dedicated `macOSInstall.yml` before attaching artifacts to the release draft. This requires an **Apple Developer Program** enrollment, which is not currently held.

## Documentation map

| Doc | Role |
|-----|------|
| This file | Platform policy + compatibility pass |
| `README.md` | User-facing supported platforms and install pointers |
| `docs/CI.md` | Which OS jobs run in GitHub Actions |
| `docs/PACKAGING_LICENSING.md` | License / SBOM / default-bundle rules |
| `docs/PRODUCTION_RUNBOOK.md` | Deploy / rollback / support |
| `AGENTS.md` | Contributor note: plugin dirs per OS |
