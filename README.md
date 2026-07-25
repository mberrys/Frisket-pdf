# Frisket-PDF

A desktop PDF toolkit for editing, inspecting, validating, and automating PDF workflows.

[Download](https://github.com/mberrys/Frisket-pdf/releases) · [Platform support](docs/PLATFORM_SUPPORT.md) · [Project guide](docs/REPO_MAP.md) · [Build from source](#build-from-source)

> **New install?** Get Frisket-PDF from the official [GitHub releases](https://github.com/mberrys/Frisket-pdf/releases). Windows and Linux are supported for V1; macOS source builds are best-effort only.

## What it does

- **Edit and inspect** PDFs with annotations, forms, attachments, optional content, text layout analysis, and document-structure tools.
- **Protect** documents with encryption, certificate management, signature validation, and digital signing.
- **Optimize and compare** files with compression, rendering, page operations, document comparison, and image/text extraction.
- **Automate** PDF work with PdfTool for batch pipelines, reporting, preflight checks, rendering, redaction, and more.

## Choose the right tool

| Tool | Use it for |
| --- | --- |
| **Pdf4QtEditor** | The primary interactive workspace for editing, inspection, and editor plugins. |
| **PdfTool** | Scripted and batch workflows in CI or from the command line. Run `PdfTool help` for commands. |
| **Pdf4QtPageMaster** | Batch page geometry, assembly, and export. |
| **Pdf4QtViewer** | Quick, read-only viewing. |
| **Pdf4QtDiff** | Comparing two PDF documents. |

## Install

Use Frisket’s own release artifacts, not upstream PDF4QT packages.

- **Windows (x64):** portable ZIP and MSI packaging are supported.
- **Linux (x64):** `.deb`, AppImage, and Flatpak packaging are available through the project’s release workflows.
- **macOS:** not supported for V1. There is no official package, notarization, or macOS CI coverage.

See [platform support](docs/PLATFORM_SUPPORT.md) for supported configurations, package layouts, and current validation notes.

## Build from source

Frisket-PDF requires a C++20 compiler, Qt 6.11.1 or newer, and vcpkg. Windows and Linux are the supported development targets.

```bash
git clone https://github.com/mberrys/Frisket-pdf.git
cd Frisket-pdf

git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT="$PWD/vcpkg"

cmake -B build -S . \
  -DPDF4QT_INSTALL_QT_DEPENDENCIES=0 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows, use Visual Studio 2022 or MinGW. For the full set of build options and packaging requirements, consult [CMakeLists.txt](CMakeLists.txt) and the [platform guide](docs/PLATFORM_SUPPORT.md).

## Documentation

- [Platform support and packaging](docs/PLATFORM_SUPPORT.md)
- [Repository map and upstream policy](docs/REPO_MAP.md)
- [Preflight tool documentation](frisket-preflight/README.md)
- [Packaging and licensing guide](docs/PACKAGING_LICENSING.md)
- [Contributor and architecture guidance](AGENTS.md)

## Contributing

Contributions, testing, feedback, and bug reports are welcome. Please read [AGENTS.md](AGENTS.md) and the [repository map](docs/REPO_MAP.md) before starting work, especially when changing the forked PDF4QT codebase or syncing upstream.

## License and acknowledgements

Frisket-PDF is based on [PDF4QT](https://github.com/JakubMelka/PDF4QT) and is currently distributed under the MIT License. **The license is subject to change at any time.** It includes third-party components with their own license obligations, including Qt, FreeType, OpenJPEG, OpenSSL, Little CMS, zlib, libjpeg, and Blend2D. Review the [packaging and licensing guide](docs/PACKAGING_LICENSING.md) before distributing a build.

Copyright © 2026 Michael Berry. Portions are copyright © 2019 The FreeType Project.
