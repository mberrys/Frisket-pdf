# Release packaging and OSS licensing (MIC-140)

Planning review for Frisket desktop distribution — **not legal advice**. A qualified open-source licensing attorney should review the final release manifest before commercial distribution.

**Linear:** [MIC-140](https://linear.app/mbx2/issue/MIC-140/plan-packaging-licensing-review-ghostscriptverapdfjre-bundle)  
**Notion source:** [MIC-140 — Packaging & Licensing Review](https://app.notion.com/p/9bdbe383233d44cd88b7916d9aa4ce6d)  
**Architecture context:** [Hybrid sidecar preflight plan (Linear)](https://linear.app/mbx2/document/hybrid-sidecar-preflight-plan-from-notion-45159ab6473e)

## Recommendation

Ship Frisket’s **default desktop bundle as C++/Qt only**: PDF4QT host + Frisket plugin + PdfTool `preflight` sidecar. Do **not** bundle Ghostscript, PDFBox, PikePDF, Python, veraPDF, or a JRE by default. Offer veraPDF + a pinned Temurin runtime only as an **optional PDF/A/PDF/UA add-on** when a client requirement justifies it.

## Decision record

| Component | Default bundle | Optional add-on | Decision |
|-----------|:--------------:|:---------------:|----------|
| Frisket / PDF4QT fork | Yes | — | MIT; re-verify at each release commit |
| Qt runtime | Yes | — | Dynamically linked LGPL-eligible modules; avoid requiring a commercial Qt license unless chosen |
| PdfTool `preflight` engine | Yes | — | Separate C++ process; no JVM. Version with plugin and JSON schema |
| Little CMS | Transitive | — | MIT; include notice |
| OpenSSL | If linked | — | Prefer 3.x (Apache 2.0); preserve notices; verify exact shipped version |
| QPDF | No for Phase 1 | Possible Phase 3 | Apache 2.0; add only when a concrete fixup requires it |
| veraPDF CLI | No | Yes | MPL 2.0 option; pin exact release; process-separated; include MPL text and controlled source location |
| Eclipse Temurin runtime | No | With veraPDF | Pin LTS supported by selected veraPDF; ship license/NOTICE unchanged; `jlink` only after license/runtime validation |
| Ghostscript | No | No without contract | Exclude from proprietary packages unless Artifex grants commercial distribution license |
| PDFBox | No | No current need | Superseded by Pdf4QtLibCore + optional veraPDF |
| PikePDF / Python | No | No current need | Dropped from architecture |

## Why the original MIC-140 issue is stale

Default checks run in PdfTool on Pdf4QtLibCore. PDFBox and Ghostscript were removed from the default path; veraPDF is optional for client-driven PDF/A or PDF/UA validation. The default engine needs **neither Java nor a JRE**.

MIC-140 is therefore a **release packaging and license-compliance gate**, not a “bundle Ghostscript/veraPDF/JRE” task:

1. **Default package:** C++/Qt only.
2. **Optional validator pack:** veraPDF + Temurin, installed separately.
3. **Ghostscript:** prohibited unless a commercial license is signed.
4. **Every release:** generate and archive an exact dependency manifest/SBOM and third-party notices.

## Proposed package layout

### Default bundle

```text
Frisket/
├─ app/
│  ├─ Pdf4QtEditor[.exe]
│  ├─ PdfTool[.exe]
│  └─ required shared libraries
├─ plugins/
│  └─ frisket-preflight[.dll|.so|.dylib]
├─ profiles/
│  ├─ frisket-default.yaml
│  └─ schemas/
├─ licenses/
│  ├─ THIRD_PARTY_NOTICES.txt
│  ├─ PDF4QT-MIT.txt
│  ├─ Qt-LGPL-3.0.txt
│  └─ component license and NOTICE files
├─ manifest/
│  ├─ components.spdx.json
│  └─ checksums.txt
└─ VERSION.json
```

User profiles (overrides): `%APPDATA%/Frisket` or MelkaJ org settings (Windows),
`~/.config/frisket` / MelkaJ (Linux), and the macOS equivalent under
`~/Library/` (confirm exact org path during MIC-336), plus bundled defaults under
`profiles/`. See [PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md).

### Optional validator pack

```text
Frisket-ValidatorPack/
├─ verapdf/
│  ├─ exact upstream CLI distribution
│  └─ version metadata
├─ runtime/
│  └─ pinned Eclipse Temurin runtime
├─ licenses/
│  ├─ MPL-2.0.txt
│  ├─ veraPDF notices
│  ├─ Temurin/OpenJDK licenses and notices
│  └─ SOURCE_OFFER.txt
└─ VERSION.json
```

`VERSION.json` should bind app, plugin ABI, PdfTool engine, report-schema version, profile-schema version, and optional validator-pack version. Refuse execution on incompatible major contract versions (see `schema_version` in [frisket-preflight/README.md](../frisket-preflight/README.md)).

## License findings

### PDF4QT and transitive runtime

PDF4QT moved from LGPLv3 to MIT (April 27, 2025) but lists third-party libraries with independent terms (Qt, FreeType, OpenJPEG, OpenSSL, Little CMS, zlib, Blend2D). Re-verify the exact fork commit and resolved binaries at release time; top-level MIT does not replace dependency obligations.

Sources: [PDF4QT](https://github.com/JakubMelka/PDF4QT), [Little CMS](https://littlecms.com/color-engine/), [OpenSSL](https://openssl-library.org/source/license/), [Blend2D](https://blend2d.com/).

### Qt (primary default-bundle decision)

Closed-source apps may use LGPLv3-eligible Qt libraries if all LGPL requirements are met: dynamic linking, recipient ability to relink against modified Qt, license text and notices, and corresponding source or valid written offer under distributor control.

**Release choice:**

- **LGPL route:** dynamic libraries, no prohibited replacement barriers, license/notices, controlled source archive or written offer, documented relinking test.
- **Commercial route:** obtain appropriate Qt commercial license before relying on commercial terms.

Sources: [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations), [Qt OSS FAQ](https://www.qt.io/faq/qt-open-source-licensing).

### veraPDF optional pack

Dual-licensed GPLv3+ or MPL 2.0+. Select **MPL 2.0** in the optional pack and document that election. Distributors must tell recipients where to obtain corresponding MPL-covered source.

Ship unmodified upstream CLI when possible. Keep Frisket integration in a separate process and separate proprietary files. Preserve veraPDF license/notice files and archive exact corresponding source.

Sources: [veraPDF licensing](https://verapdf.org/home/), [veraPDF apps](https://github.com/veraPDF/veraPDF-apps), [MPL FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/).

### JRE / Eclipse Temurin

Adoptium Temurin binaries are GPLv2 with Classpath Exception. Bundle a pinned supported LTS runtime only with the optional validator pack. Preserve all license/NOTICE files. Do not accidentally ship Oracle JDK.

Prefer smallest operationally safe option:

1. Test veraPDF against unmodified Temurin runtime package.
2. If size matters, build a `jlink` runtime in CI from pinned Temurin JDK.
3. Compare validator results and startup across supported platforms.
4. Archive exact JDK input, module list, build command, notices, and source location.

Sources: [Adoptium FAQ](https://adoptium.net/docs/faq), [OpenJDK GPLv2 + CE](https://openjdk.org/legal/gplv2+ce.html).

### Ghostscript

AGPL or Artifex commercial license. Since Ghostscript is not needed for the default architecture, **complete exclusion** is the lowest-risk decision.

**Hard gate:** fail packaging if a Ghostscript executable, library, resource, installer, or container layer appears in a proprietary Frisket artifact unless the release record contains an executed Artifex commercial agreement.

Sources: [Ghostscript licensing](https://ghostscript.com/licensing), [Artifex licensing](https://artifex.com/licensing).

### QPDF, PDFBox, PikePDF

QPDF and PDFBox: Apache 2.0. PikePDF: MPL 2.0. Manageable licenses, but **do not ship unused dependencies**. Add any component only with a documented feature requirement and dependency review.

Sources: [QPDF license](https://qpdf.readthedocs.io/en/stable/license.html), [PDFBox](https://pdfbox.apache.org/), [PikePDF](https://github.com/pikepdf/pikepdf).

## Build and release controls

- [ ] Pin every source commit, binary version, download URL, and SHA-256 hash
- [ ] Produce SPDX or CycloneDX SBOM from the **final packaged artifact** — not only the source tree
- [ ] Scan installer/archive for undeclared executables, shared libraries, JARs, Python wheels, fonts, ICC profiles, and data files
- [ ] Generate `THIRD_PARTY_NOTICES.txt` from resolved manifest; manual review
- [ ] Archive every license and NOTICE file exactly as shipped
- [ ] Verify PDF4QT upstream/fork license at release commit
- [ ] Audit every used Qt module and plugin; choose LGPL compliance or commercial license
- [ ] Under LGPL, test recipient can replace/relink Qt libraries and still launch the app
- [ ] Keep Qt and other corresponding source archives or written offers under Berry Studio control
- [ ] Ensure default installer contains **no** Ghostscript, veraPDF, JRE/JDK, PDFBox, PikePDF, or Python payload
- [ ] If validator pack ships: pin veraPDF + Temurin as one tested unit; MPL/OpenJDK source information
- [ ] Run dependency vulnerability and EOL checks; record exceptions
- [ ] Sign artifacts and publish checksums
- [ ] Counsel approves final manifest before first paid external distribution and after any copyleft/commercial-license change

## Acceptance criteria (MIC-140)

- [ ] Default package definition is C++/Qt only and documented for Windows, Linux, and macOS ([PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md) / MIC-336)
- [ ] Plugin, PdfTool engine, schemas, and profiles share one compatibility/version contract
- [ ] Qt licensing route selected with compliance evidence stored with the release
- [ ] Ghostscript exclusion enforced by automated artifact scan
- [ ] Optional veraPDF/Temurin pack has pinned version pair, license bundle, source-location record, checksums, and smoke tests
- [ ] SBOM and third-party notices generated from final artifact
- [ ] Clean-machine install, upgrade, rollback, and uninstall pass without system Java or other prerequisites
- [ ] Final legal review complete before commercial ship

## Suggested Linear issue rewrite

**Title:** `[Plan] Release packaging + OSS licensing gate (Qt / optional veraPDF)`

**Summary:** Default Frisket packaging is C++/Qt only: PDF4QT host, Frisket plugin, and PdfTool preflight sidecar. No JRE, Ghostscript, PDFBox, PikePDF, or Python in the default installer. veraPDF + Eclipse Temurin may ship as a separately versioned optional validator pack for client-required PDF/A/PDF/UA validation. Ghostscript is prohibited unless covered by an executed Artifex commercial distribution license.

**Deliverables:**

- Installer/package layout and compatibility manifest
- Qt LGPL-vs-commercial decision with compliance evidence
- Automated forbidden-component scan
- Optional veraPDF/Temurin pack proof of concept
- Final-artifact SBOM and third-party notices
- Clean-machine packaging tests
- Pre-commercial legal sign-off
