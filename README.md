# Frisket-PDF

[![CI](https://github.com/mberrys/Frisket-pdf/actions/workflows/ci.yml/badge.svg)](https://github.com/mberrys/Frisket-pdf/actions/workflows/ci.yml)

(c) Michael Berry 2026 Frisket PDF

*Software works on Microsoft Windows and Linux.*

Supported platforms and the packaging compatibility pass are documented in [docs/PLATFORM_SUPPORT.md](docs/PLATFORM_SUPPORT.md).

Software is provided without any license or warranty of any kind. This software also uses several third-party libraries, and users must comply with the licenses of those third-party components.

## 1. ACKNOWLEDGEMENTS

a. This software is based on Jakub Melka's PDF4QT and is licensed under the MIT license. 

Portions of this software are copyright © 2019 The FreeType  
Project ([www.freetype.org](http://www.freetype.org)). All rights reserved.

## 2. FEATURES

Software have following features (the list is not complete):

- [x] multithreading support
- [x] encryption
- [x] color management
- [x] optional content handling
- [x] text layout analysis
- [x] signature validation
- [x] annotations
- [x] form filling
- [x] text to speech capability
- [x] editation
- [x] file attachments
- [x] optimalization (compressing documents)
- [x] command line tool
- [x] audio book conversion
- [x] internal structure inspector
- [x] compare documents
- [x] static XFA support (readonly, simple XFA only)
- [x] electronically/digitally sign documents
- [x] public key security encryption

## 3. ARCHITECTURE

Frisket-PDF uses multiple apps on one core library:

- **Pdf4QtEditor** — primary interactive app; hosts editor plugins (preflight, ObjectInspector, etc.)
- **PdfTool** — command-line companion for pipelines and batch automation
- **Pdf4QtPageMaster** — batch page geometry, assembly, and export (including bleed-box settings)
- **Pdf4QtViewer** — lighter read-only viewer (not the primary product shell)

See `AGENTS.md` for contributor and agent placement rules.

## 4. THIRD PARTY LIBRARIES

Several third-party libraries are used.

1. libjpeg, see [https://www.ijg.org/](https://www.ijg.org/)
2. FreeType, see [https://www.freetype.org/index.html](https://www.freetype.org/index.html), FTL license used
3. OpenJPEG, implementing Jpeg2000, see [https://www.openjpeg.org/](https://www.openjpeg.org/), 2-clause MIT license
4. Qt, [https://www.qt.io/](https://www.qt.io/), LGPL license used
5. OpenSSL, [https://www.openssl.org/](https://www.openssl.org/), Apache 2.0 license
6. LittleCMS, [http://www.littlecms.com/](http://www.littlecms.com/)
7. zlib, [https://zlib.net/](https://zlib.net/)
8. Blend2D, [https://blend2d.com/](https://blend2d.com/)

8. Blend2D, [https://blend2d.com/](https://blend2d.com/)

## 5. CONTRIBUTIONS

Fork and upstream policy: see [docs/REPO_MAP.md](docs/REPO_MAP.md).

Contributions are welcome!

Since the project is now licensed under the MIT License, contributions can be freely submitted without the need to sign a Contributor License Agreement (CLA).
However, all contributions must be made under the terms of the MIT License to ensure license consistency across the project.

You are encouraged to contribute by testing, offering feedback, providing advice, or submitting code improvements.

## 6. INSTALLING

Frisket V1 supports **Windows** and **Linux**. Prefer Frisket GitHub
[Releases](https://github.com/mberrys/Frisket-pdf/releases) over upstream PDF4QT
binaries. Layout and smoke-test expectations: [docs/PLATFORM_SUPPORT.md](docs/PLATFORM_SUPPORT.md).

Executable names: Pdf4QtEditor, Pdf4QtDiff, Pdf4QtLaunchPad, Pdf4QtPageMaster,
Pdf4QtViewer, and PdfTool.

### Windows

CI produces a portable zip; MSI packaging is built via `WindowsInstall.yml`
(`WixInstaller/`). Clean-machine validation is tracked in MIC-301.

### Linux

- **`.deb`:** produced from the Ubuntu CI job (`make-package.sh`).
- **AppImage:** `LinuxInstall.yml` (manual dispatch).
- **Flatpak:** `LinuxFlatpak.yml` / `Flatpak/io.github.mberrys.Frisket-pdf.json`
  (note: current Flatpak grants `--filesystem=host` — see MIC-328).

Upstream AUR / Flathub packages for PDF4QT are not Frisket release artifacts.

### macOS

**Not supported for V1.** There is no macOS CI job, no official DMG/pkg, and no
notarization, so no release assets are published for macOS. The sources are
expected to build with a recent Xcode / Apple Clang, but that path is
best-effort and unverified — see [docs/PLATFORM_SUPPORT.md](docs/PLATFORM_SUPPORT.md).

## 7. COMPILING

This software can be compiled on Windows and Linux. A compiler supporting the C++20 standard is needed. macOS builds are best-effort and not covered by CI.

On Windows, you can use Visual Studio 2022 or MinGW.

On Linux, a GCC version >= 8 should work, although we tested it with GCC 11.

On macOS, a recent Xcode / Apple Clang with Qt 6.9 and vcpkg is expected to work, but this configuration is not built or tested in CI.

### Compiling from sources

1. Install [vcpkg](https://vcpkg.io/en/getting-started.html)

        git clone https://github.com/Microsoft/vcpkg.git
        ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
        VCPKG_ROOT=$(pwd)/vcpkg

    Check that vcpkg path is correct: `$VCPKG_ROOT/vcpkg --version`.

2. Build Frisket-PDF

    2.1 Clone repo

        git clone https://github.com/mberrys/Frisket-pdf
        cd Frisket-pdf

    2.2 Configure

        cmake -B build -S . -DPDF4QT_INSTALL_QT_DEPENDENCIES=0 -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_INSTALL_PREFIX='/' -DCMAKE_BUILD_TYPE=Release

   (One user reported success with `-DCMAKE_INSTALL_PREFIX=''` instead, as otherwise all there paths were prepended with `/usr` (causing `/usr/usr...`

    For a debug build, append `-DCMAKE_BUILD_TYPE=Debug`.

    It is recommended to set the VCPKG_OVERLAY_PORTS variable to 'PDF4QT/vcpkg/overlays' to prevent crashes due to the incompatible LIBPNG library on some Linux systems.

    2.3 Build

        cmake --build build

    Use the [`-j` switch](https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-build-j) to build multiple files in parallel.

    2.4 Install

        sudo cmake --install build

    To uninstall, run `sudo xargs rm < ./build/install_manifest.txt`.

### Using Qt Creator (Windows / Linux / macOS)
1. Download Qt 6.9 or higher, and VCPKG package manager (https://vcpkg.io/en/index.html)
2. Open Qt Creator and configure the project
3. Build
 
### CMAKE Compilation Options

Several important compilation options are available and should be set before building. On Windows,
CMake can prepare a Wix project to create a *.msi installer package.

|                  Option                | Platform |     Description                                          |
| ------------------------------------   | ---------|--------------------------------------------------------- |
| `PDF4QT_INSTALL_MSVC_REDISTRIBUTABLE`  | Windows  |Includes MSVC redistributable in installation             |
| `PDF4QT_INSTALL_PREPARE_WIX_INSTALLER` | Windows  |Prepare .msi installator using Wix installer              |
| `PDF4QT_INSTALL_DEPENDENCIES`          | Any      |Install dependent libraries into installation directory   |
| `PDF4QT_INSTALL_QT_DEPENDENCIES`       | Any      |Install Qt dependent libraries into installation directory|
| `VCPKG_OVERLAY_PORTS`                  | Linux    |Set it to prevent crashes with incompatible libpng library|
 
Following important variables should be set or checked before any attempt to compile this project:

|                  Variable              | Platform |     Description                                          |
| ------------------------------------   | ---------|--------------------------------------------------------- |
| `PDF4QT_QT_ROOT`                       | Any      |Qt installation directory                                 |
| `QT_CREATOR_SKIP_VCPKG_SETUP`          | Any      |Enable or disable automatic vcpkg setup                   |
| `CMAKE_PROJECT_INCLUDE_BEFORE`         | Any      |Should be set to package manager auto setup               |
| `CMAKE_TOOLCHAIN_FILE`                 | Any      |Should be set to toolchain                                |
| `CMAKE_BUILD_TYPE`                     | Any      |Can be Release (default) or Debug                         |

#### Sample setup on Windows

Following set of variables gives sample setup for MS Windows. It is minimal initial configuration
to be able to built Debug build on MS Windows.

| Key                             | Value                                                        |
| ------------------------------- | -------------------------------------------------------------|
| `CMAKE_BUILD_TYPE`              | Debug                                                        |
| `CMAKE_CXX_COMPILER`            | %{Compiler:Executable:Cxx}                                   |
| `CMAKE_C_COMPILER`              | %{Compiler:Executable:C}                                     |
| `CMAKE_GENERATOR`               | Ninja                                                        |
| `CMAKE_PREFIX_PATH`             | %{Qt:QT_INSTALL_PREFIX}                                      |
| `CMAKE_PROJECT_INCLUDE_BEFORE`  | %{IDE:ResourcePath}/package-manager/auto-setup.cmake         |
| `CMAKE_TOOLCHAIN_FILE`          | %{Qt:QT_INSTALL_PREFIX}/lib/cmake/Qt6/qt.toolchain.cmake     |
| `PDF4QT_QT_ROOT`                | C:/Programming/Qt/6.4.0/msvc2019_64                          |
| `QT_QMAKE_EXECUTABLE`           | %{Qt:qmakeExecutable}                                        |


### Tested Compilers - Windows
 - Visual Studio 2022 (Microsoft Visual C++ Compiler 17.1)
 - MinGW 11.2.0
 
### Tested Compilers - Linux
 - GCC 13.1.1

## 8. DISCLAIMER

I wrote this project in my free time. I hope you will find it useful!

