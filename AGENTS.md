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

**PageMaster is not a plugin host.** It uses its own `MainWindow` / workspace model (`pageitemmodel`, assembly export). It already persists page-geometry settings (including `applyBleedBox`) in export JSON — keep batch page/bleed/imposition work here, not in Editor plugins.

**PdfTool is not an interactive shell.** Extend it for throughput (scripted checks, renders, optimizations), not as a GUI replacement.

**Placement rules for new work:**

- Interactive plugin or single-document tool → **Editor** (+ `Pdf4QtEditorPlugins/`)
- Batch page geometry, bleed, assembly across many pages/docs → **PageMaster**
- Scripted / pipeline / unattended operation → **PdfTool** (+ `Pdf4QtLibCore/`)
- Shared PDF logic used by multiple surfaces → **Core** (and `Pdf4QtLibWidgets/` / `Pdf4QtLibGui/` if UI helpers are needed)

Do not make Viewer the primary shell or add plugin hosting to PageMaster without an explicit architecture change.

## Build & configure

- Do not run project builds, CMake configure/reconfigure, vcpkg install, or full solution rebuilds unless the user explicitly asks in the current conversation.
- When a build is requested, prefer compiling only the touched target (e.g. `Pdf4QtLibCore`, `PdfTool`, `UnitTests`) over rebuilding everything.
- Prefer an existing build directory; do not invent new out-of-tree build folders or change toolchain/generator settings unless asked.
- Do not add, upgrade, or remove third-party dependencies (vcpkg ports, CMake `find_package`, overlays) without asking.

## Editing hygiene

- Preserve CRLF line endings when creating or editing source and text files.
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

- Do not commit, push, amend, or rewrite history unless the user explicitly asks.
- Do not edit vendored or third-party trees (e.g. font bundles, vcpkg overlay ports) unless the task is specifically about those files.
