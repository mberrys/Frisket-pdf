# Frisket-PDF

[![CI](https://github.com/mberrys/Frisket-pdf/actions/workflows/ci.yml/badge.svg)](https://github.com/mberrys/Frisket-pdf/actions/workflows/ci.yml)

(c) Michael Berry 2026 Frisket PDF

*Software works on Microsoft Windows / Linux.*

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
