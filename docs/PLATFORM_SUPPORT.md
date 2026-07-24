# Platform support (Windows, Linux, macOS)



Frisket PDF V1 targets three desktop platforms. This document is the source of

truth for supported OS, install layouts, and the **cross-platform compatibility

pass** over modules that are already feature-complete.



**Linear:** [MIC-336](https://linear.app/mbx2/issue/MIC-336) (R-014)  

**Related:** [MIC-301](https://linear.app/mbx2/issue/MIC-301) (Windows MSI),

[MIC-328](https://linear.app/mbx2/issue/MIC-328) (Flatpak permissions),

[MIC-329](https://linear.app/mbx2/issue/MIC-329) (licensing checklist),

`docs/PACKAGING_LICENSING.md`



## Supported platforms



| Platform | Status | CI | Official packages |

|----------|--------|----|-------------------|

| **Windows** (x64) | Supported | `ci.yml` + `WindowsInstall.yml` | MSI (`WixInstaller/`), portable zip |

| **Linux** (x64) | Supported | `ci.yml` + `LinuxInstall.yml` / `LinuxFlatpak.yml` | `.deb`, AppImage, Flatpak |

| **macOS** (Apple Silicon + Intel as CI allows) | **Supported (in progress)** | Add `macos` job to `ci.yml` (MIC-336) | DMG and/or `.pkg` + notarization |



Unsupported for V1: iOS, Android, other BSDs. Community builds elsewhere are best-effort only.



## Install and plugin layout



| Platform | Binaries | Plugins (`PDF4QT_PLUGINS_DIR`) | Preflight profiles |

|----------|----------|--------------------------------|--------------------|

| Windows | `<prefix>/bin` (beside MSI tree) | `<prefix>/pdfplugins` (relative `../pdfplugins`) | `share/frisket/profiles/` in bundle |

| Linux | `<prefix>/bin` | `<prefix>/lib/pdf4qt` | `/usr/share/frisket/profiles/` (deb) |

| macOS | App bundle `Contents/MacOS` (preferred) or install prefix | Non-Linux CMake path uses `pdfplugins` today — verify inside `.app` / Frameworks layout during MIC-336 | Bundle `Contents/Resources/frisket/profiles/` or documented share path |



Editor must resolve **PdfTool** and **FrisketPreflightPlugin** without a developer toolchain on PATH.



## V1 slim distribution



When `PDF4QT_FRISKET_DISTRIBUTION=ON`, prefer Editor + PdfTool + core plugins

(FrisketPreflight and required inspection plugins). PageMaster / Diff / Viewer /

LaunchPad may ship in full packages; still build them in CI on all three OS.



## Cross-platform compatibility pass



Run this pass before marketing V1 on any platform. Goal: correct **dependency

bundling** and **installer packaging** for modules that are already complete.



### Module checklist



| Module / surface | Already complete? | Win | Linux | macOS | What to verify |

|------------------|-------------------|:--:|:-----:|:-----:|----------------|

| Pdf4QtLibCore | Yes | ☐ | ☐ | ☐ | Qt 6.9 + vcpkg build; codecs/fonts; no Widgets |

| Pdf4QtLibWidgets / Pdf4QtLibGui | Yes | ☐ | ☐ | ☐ | Plugin relative path; MelkaJ settings paths |

| PdfTool (`preflight`, `add-bleed`, …) | Yes | ☐ | ☐ | ☐ | Bundled next to Editor; working directory; offscreen CI |

| FrisketPreflightPlugin | Yes | ☐ | ☐ | ☐ | Finds PdfTool + `frisket-default.json`; `.dll` / `.so` / `.dylib` |

| Pdf4QtEditor | Yes | ☐ | ☐ | ☐ | Clean-machine launch; plugins load; operator loop |

| Other Editor plugins | Yes | ☐ | ☐ | ☐ | Present in intended bundle set; load without system Qt |

| Pdf4QtPageMaster export (MIC-307–312) | Yes | ☐ | ☐ | ☐ | Atomic write + manifest; cancel; case-sensitive FS |

| Pdf4QtViewer / Diff / LaunchPad | Adjacent | ☐ | ☐ | ☐ | Build in CI; optional in slim package |

| frisket-preflight profiles + schemas | Yes | ☐ | ☐ | ☐ | Installed at documented path; schema version contract |

| UnitTests (operator, corpus, PageMaster) | Yes | ☐ | ☐ | ☐ | `ctest` green on all three CI runners |

| Windows MSI | In review (MIC-301) | ☐ | — | — | Clean VM smoke; redist; signing |

| Linux `.deb` / AppImage / Flatpak | Exists | — | ☐ | — | Smoke; Flatpak `--filesystem=host` documented |

| macOS DMG / pkg | Missing | — | — | ☐ | `macdeployqt` (or equiv.); codesign; notarize; smoke |

| Sentry (optional) | Partial | ☐ | ☐ | ☐ | Opt-in DSN only; DB path; no default PII |

| OCR sidecar (optional) | Not V1-gated | ☐ | ☐ | ☐ | Bundled-only guidance; do not block platform gate |



### Bundling rules (all OS)



1. Ship Qt runtime and required Qt plugins (`platforms`, `imageformats`, …) inside the package — do not require a system Qt install.

2. Co-locate PdfTool, FrisketPreflightPlugin, and `frisket-default.json` per the layout table.

3. Keep the default bundle C++/Qt only (see `docs/PACKAGING_LICENSING.md`); scan for forbidden Ghostscript / JRE / Python payloads.

4. Codesign where the platform expects it (Authenticode, Apple Developer ID + notarization).

5. Document upgrade, uninstall, and binary rollback (no cloud DB).



### Smoke test (every installer)



1. Clean machine (no Qt / MSVC / Xcode command-line tools required at runtime).

2. Install the platform package.

3. Launch Pdf4QtEditor.

4. Open a sample PDF; run Frisket Preflight; confirm findings JSON.

5. Confirm PdfTool exists beside the app and profiles resolve.



## Documentation map



| Doc | Role |

|-----|------|

| This file | Platform policy + compatibility pass |

| `README.md` | User-facing supported platforms and install pointers |

| `docs/CI.md` | Which OS jobs run in GitHub Actions |

| `docs/PACKAGING_LICENSING.md` | License / SBOM / default-bundle rules |

| `docs/PRODUCTION_RUNBOOK.md` | Deploy / rollback / support (when present on branch) |

| `AGENTS.md` | Contributor note: plugin dirs per OS |



## Implementation notes (macOS)



- Apps already set `MACOSX_BUNDLE ON` for Editor, Viewer, PageMaster, Diff, LaunchPad.

- CMake today treats non-`PDF4QT_LINUX` like Windows for `PDF4QT_PLUGINS_DIR` (`pdfplugins`). Confirm that path inside a `.app` bundle or adjust install rules as part of MIC-336.

- Prefer GitHub `macos-latest` (or a pinned image) with Qt 6.9 and vcpkg; mirror Ubuntu/Windows `ctest` set.

- Notarization and staple steps belong in a dedicated `macOSInstall.yml` (or equivalent) before attaching artifacts to the release draft.


