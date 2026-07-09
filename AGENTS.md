# Agent Instructions

Frisket-PDF is a fork of PDF4QT — a Qt6 / C++20 PDF toolkit (core library, widgets, GUI apps, CLI tools, editor plugins). Prefer minimal, local changes that match nearby code.

## Frisket / project facts

| Item | Value |
|------|-------|
| **Fork** | [mberrys/Frisket-pdf](https://github.com/mberrys/Frisket-pdf) ← [JakubMelka/PDF4QT](https://github.com/JakubMelka/PDF4QT) |
| **Version** | `1.6.0.0` (`PDF4QT_VERSION` in root `CMakeLists.txt`) |
| **Language** | C++20 |
| **Qt** | 6.9 (CI installs **6.9.0** via `jurplel/install-qt-action`) |
| **Dependencies** | vcpkg manifest: `vcpkg.json`, `vcpkg-configuration.json`, overlays in `vcpkg/overlays/` |
| **Upstream sync** | On-demand GitHub **Sync fork** only; do not merge upstream unless asked |

**Editor plugin directory** (`PDF4QT_PLUGINS_DIR` in root `CMakeLists.txt`):

- **Windows:** `<install-prefix>/pdfplugins` (relative to `bin`: `../pdfplugins`)
- **Linux:** `<install-prefix>/pdf4qt` (under the install lib dir)

Plugins are built from `Pdf4QtEditorPlugins/` (AudioBook, Dimensions, Editor, ObjectInspector, OutputPreview, Redact, Scanner, Signature, SoftProofing).

**PdfTool** (`PdfTool/`): CLI entry point is `PdfTool <command> [options]`. Run `PdfTool help` for the full list. Common commands:

| Command | Purpose |
|---------|---------|
| `info` | Document summary |
| `info-metadata` | XMP/metadata |
| `info-fonts` | Embedded fonts |
| `render` | Render pages to images |
| `fetch-text` | Extract text |
| `fetch-images` | Extract images |
| `optimize` | Compress/optimize PDF |
| `unite` / `separate` | Merge or split documents |
| `diff` | Compare two PDFs |
| `redact` | Apply redactions |
| `encrypt` / `decrypt` | Password protection |
| `verify-signatures` | Check digital signatures |
| `attachments` | List/save embedded files |
| `statistics` | Document statistics |

Shared options on most commands: `--pswd`, page range flags (`--page-first`, `--page-last`, `--page-select`), and `--console-format text|xml|html`.

## Application architecture (confirmed)

Frisket uses a **split surface** model — one core library (`Pdf4QtLibCore`), multiple apps tuned for different jobs. Do not collapse these roles into a single shell.

| App | Path | Role | Use when |
|-----|------|------|----------|
| **Pdf4QtEditor** | `Pdf4QtEditor/` + `Pdf4QtLibGui/` | **Primary interactive shell** | Daily work, preflight UI, inspection, editing, **all new editor plugins** |
| **PdfTool** | `PdfTool/` | **Headless automation companion** | CI, batch pipelines, render/optimize/info/diff/redact without GUI |
| **Pdf4QtPageMaster** | `Pdf4QtPageMaster/` | **Batch geometry / assembly** | Multi-document page workspaces, crop/assemble/export, bleed/trim/media box batch apply |
| **Pdf4QtViewer** | `Pdf4QtViewer/` + `Pdf4QtLibGui/` | Lighter read-only viewer | Quick viewing only — **not** the Frisket product shell |

**Editor is the only plugin host.** `PDFProgramController` loads plugins only when the `Plugins` feature flag is set. Editor initializes with `AllFeatures`; Viewer initializes with `TextToSpeech | Tools` only (no plugins). New Frisket capabilities (preflight, ObjectInspector extensions, future Ocr/Bleed plugins) belong in `Pdf4QtEditorPlugins/` and are loaded from `PDF4QT_PLUGINS_DIR`.

**PageMaster is not a plugin host.** It uses its own `MainWindow` / workspace model (`pageitemmodel`, assembly export). It already persists page-geometry settings (including `applyBleedBox`) in export JSON — note `applyBleedBox` is **box rewrite only**, not artwork bleed generation. Keep batch page/bleed/imposition work here, not in Editor plugins.

**PdfTool is not an interactive shell.** Extend it for throughput (scripted checks, renders, optimizations), not as a GUI replacement. Destructive fixups stay non-interactive (`--force` / `--dry-run` / `--report`); confirmation dialogs belong in PageMaster or Editor only.

**Placement rules for new work:**

- Interactive plugin or single-document tool → **Editor** (+ `Pdf4QtEditorPlugins/`)
- Batch page geometry, bleed, assembly across many pages/docs → **PageMaster**
- Scripted / pipeline / unattended operation → **PdfTool** (+ `Pdf4QtLibCore/`)
- Shared PDF logic used by multiple surfaces → **Core** (and `Pdf4QtLibWidgets/` / `Pdf4QtLibGui/` if UI helpers are needed)

Do not make Viewer the primary shell or add plugin hosting to PageMaster without an explicit architecture change.

## Page boxes and prepress fixups

- `PDFPageGeometry` / PageMaster `applyBleedBox` rewrite box metadata (and optional content scale). They do **not** generate bleed artwork.
- Artwork bleed / edge-extend fixups belong in Core as their own `apply()` API, surfaced via PdfTool first, then PageMaster batch export; Editor confirm UI is optional and later. See [docs/MIRROR_BLEED_PLAN.md](docs/MIRROR_BLEED_PLAN.md) (`PDFBleedFixup` / `PdfTool add-bleed`).
- Default box nesting when expanding for production: grow MediaBox, CropBox, and BleedBox together; keep TrimBox fixed unless the feature explicitly says otherwise. Missing Bleed/Trim/Art often default to CropBox in this codebase.
- If injecting page content with `PDFPageContentStreamBuilder`, expand MediaBox before `begin()` (paint surface is sized from the current MediaBox). Prefer `PlaceBefore` for underlays, `PlaceAfter` for overlays.
- PageMaster export order for stacked ops: assemble → page geometry → content fixups → image optimize → write.
- Prefer one Core fixup API with a mode enum when variants share box/render/placement plumbing (e.g. mirror vs pixel-repeat vs stretch).

## Planning related multi-surface features

When two or more issues/modes will share one Core API (or one PdfTool command):

1. Lock naming, settings shape, box policy, and surface order in a docs plan **before** coding (M0).
2. Update Linear issues to match; set blocked-by when a later mode depends on shared scaffolding.
3. Land the shared `apply()` + first mode first; add sibling modes as strip/transform variants only — do not fork a second pipeline.
4. Prefer amending the plan doc over renaming types mid-implementation.

Process detail: [docs/PLANNING.md](docs/PLANNING.md).

## Build & configure

- Do not run project builds, CMake configure/reconfigure, vcpkg install, or full solution rebuilds unless the user explicitly asks in the current conversation.
- When a build is requested, prefer compiling only the touched target (e.g. `Pdf4QtLibCore`, `PdfTool`, `UnitTests`) over rebuilding everything.
- Prefer an existing build directory; do not invent new out-of-tree build folders or change toolchain/generator settings unless asked.
- Do not add, upgrade, or remove third-party dependencies (vcpkg ports, CMake `find_package`, overlays) without asking.

## Editing hygiene

- Preserve CRLF line endings when creating or editing source and text files.
- Preflight contract: profile + report schemas live under `frisket-preflight/` (see that README). Do not invent alternate report shapes in the Qt plugin.
- Match existing include order, naming, brace style, and file layout; do not reformat unrelated code or run mass clang-format.
- Prefer minimal diffs: no drive-by cleanups, renames, header reshuffles, or “modernization” of untouched code.
- Keep the MIT license header on new `.h` / `.cpp` files, matching neighboring files.
- Do not hand-edit generated artifacts (moc/uic/rcc outputs, `config.h`, export headers under the build tree) unless the task explicitly requires it.

## C++ / Qt conventions

- Language standard is C++20 (`CMAKE_CXX_STANDARD 20`). Do not introduce C++23-only features or change the dialect.
- Project defines `QT_NO_EMIT`; do not rely on the `emit` macro—use the same signal/slot invocation style as nearby code.
- Library code lives under `namespace pdf`. Follow existing `pdf*.h` / `pdf*.cpp` naming in library modules.
- Prefer Qt types and patterns already used in the touched module (`QString`, Qt containers, `QObject` parent ownership in GUI code, etc.).
- Keep headers lean: avoid heavy includes in `.h` when nearby code uses forward declarations; put implementation includes in `.cpp`.
- AUTOMOC / AUTOUIC / AUTORCC are enabled—do not add manual moc steps or duplicate generated wiring.

## Module boundaries

| Area | Path | Notes |
|------|------|--------|
| Core PDF library | `Pdf4QtLibCore/` | No Qt Widgets. UI-free; Qt Core/Gui only as already used. |
| Widgets | `Pdf4QtLibWidgets/` | Widget helpers/tools; depends on Core. |
| Shared GUI | `Pdf4QtLibGui/` | Shared editor/viewer GUI pieces. |
| Apps | `Pdf4QtEditor/`, `Pdf4QtViewer/`, `Pdf4QtPageMaster/`, `Pdf4QtDiff/`, `Pdf4QtLaunchPad/` | See **Application architecture** — Editor = primary shell + plugins; PageMaster = batch geometry; Viewer = read-only |
| CLI | `PdfTool/` | Headless pipelines; not the interactive shell |
| Plugins | `Pdf4QtEditorPlugins/` | Editor-only; loaded via `PDFProgramController::loadPlugins()` |
| Tests | `UnitTests/` | Qt Test executables (`UnitTests`, `UnitTestsImageOptimizer`). |
| CMake / deps | root `CMakeLists.txt`, `cmake/`, `vcpkg/` | Touch only when the task requires build-system changes. |

- Do not pull Widgets/GUI dependencies into `Pdf4QtLibCore`.
- When unsure where code belongs, follow the dependency direction already used by similar features (Core ← Widgets/Gui/Apps/Tools).

## Tests

- Unit tests live in `UnitTests/` and use Qt Test (`Qt6::Test`).
- Add or extend tests next to existing `tst_*.cpp` patterns; wire new sources in `UnitTests/CMakeLists.txt` the same way as current targets.
- Do not run the test suite unless the user asks.

## Git & safety

- Upstream tracking: see [docs/REPO_MAP.md](docs/REPO_MAP.md) for sync workflow, conflict handling, and fork-only files. Pull from upstream only when explicitly asked.
- Do not commit, push, amend, or rewrite history unless the user explicitly asks.
- Do not edit vendored or third-party trees (e.g. font bundles, vcpkg overlay ports) unless the task is specifically about those files.

## Cursor Cloud specific instructions

Environment is prebuilt into the VM snapshot; the startup update script only refreshes vcpkg deps. Build/run tooling is not on `PATH` by default in non-login shells — the exports below live in `~/.bashrc`, so use a login/interactive shell or re-source it if a command "can't find Qt/vcpkg".

**Prebuilt locations (persisted in snapshot, outside `/workspace`):**

- Qt **6.9.1** (gcc_64) at `/opt/Qt/6.9.1/gcc_64` (installed via `aqtinstall`).
- vcpkg at `/opt/vcpkg`; installed deps at `/opt/vcpkg_installed` (`x64-linux`, static libs).
- `~/.bashrc` exports: `PDF4QT_QT_ROOT`, `QT_ROOT_DIR`, `CMAKE_PREFIX_PATH`, `LD_LIBRARY_PATH` (Qt), `VCPKG_ROOT`, `VCPKG_INSTALLED_DIR=/opt/vcpkg_installed`, `VCPKG_OVERLAY_PORTS=/workspace/vcpkg/overlays/linux:/workspace/vcpkg/overlays/general`, `CC=gcc`, `CXX=g++`, and `QT_QPA_PLATFORM=offscreen`.

**Compiler gotcha:** `/usr/bin/c++` is misconfigured clang (looks for gcc-14 libstdc++ that is not installed) — plain `cmake`/`vcpkg` will fail with `cannot find -lstdc++`. This is a **GCC** build; `CC=gcc`/`CXX=g++` (gcc-13) are exported to force it. Keep them set.

**Configure (existing build dir is `/workspace/build`, Ninja):**
```
cmake -B build -S . -G Ninja -DPDF4QT_INSTALL_QT_DEPENDENCIES=0 \
  -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_INSTALLED_DIR=/opt/vcpkg_installed -DVCPKG_MANIFEST_INSTALL=OFF \
  -DCMAKE_BUILD_TYPE=Release -DPDF4QT_QT_ROOT=/opt/Qt/6.9.1/gcc_64
```
Build all: `cmake --build build --target all release_translations -j$(nproc)` (~5 min on 4 cores). Prefer single targets (e.g. `--target Pdf4QtLibCore`, `PdfTool`, `UnitTests`) per repo rules.

**Test:** `cd build && ctest --output-on-failure` (runs `UnitTests`, `UnitTestsImageOptimizer`). CI itself does not run ctest. No separate lint step exists — compiler warnings are the only static checks.

**Run:** Binaries land in `build/usr/bin`, plugins in `build/usr/lib/pdf4qt` (Editor finds them via the relative `../lib/pdf4qt` path).
- Headless CLI: `PdfTool <command> ...` (works with the default `QT_QPA_PLATFORM=offscreen`).
- GUI apps (Pdf4QtEditor, Viewer, PageMaster, Diff): a VNC X server is on `DISPLAY=:1`. Run with `DISPLAY=:1` and `QT_QPA_PLATFORM` unset (or `=xcb`) — otherwise the offscreen default gives no visible window. Harmless `qt.multimedia ... pipewire-0.3` warnings appear (no audio backend) and can be ignored.
- No sample PDFs ship in the repo; generate some with `PdfExampleGenerator` (writes `Ex_*.pdf` into the CWD).
