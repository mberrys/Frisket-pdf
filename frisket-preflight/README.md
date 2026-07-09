# frisket-preflight

Standalone preflight **engine** for Frisket (sidecar CLI). Lives outside the PDF4QT / Frisket Editor process. The Qt plugin (Phase 2) shells out to this tool and only consumes the normalized report JSON defined here.

This directory currently locks the **contract** (MIC-131). The CLI binary itself is MIC-133.

## Layout

| Path | Purpose |
|------|---------|
| `schemas/profile.schema.json` | JSON Schema for declarative profiles (YAML or JSON) |
| `schemas/report.schema.json` | JSON Schema for stdout report JSON (`bbox` required on every finding) |
| `profiles/frisket-default.yaml` | Bundled **Frisket Default** profile (Venue Poster / Handbill / Signage) |
| `examples/report.example.json` | Example failing report matching the report schema |

## Profile (YAML/JSON)

Required:

- `name` — human-readable; copied into `report.profile`
- `checks[]` — each entry has `id`, optional `severity`, and check-specific params

Optional:

- `schema_version` — currently `1`
- `job_types[]` — Frisket job categories
- `description`
- `fixups[]` — each has `id`, `confirm` (default true), and params

Check params used by Phase 1 plans (open-ended via `additionalProperties`):

| Param | Used by |
|-------|---------|
| `min_dpi` | `image-resolution` |
| `amount_pt` | `bleed`, `add-bleed` |
| `required` | `bleed` |
| `allowed` | `color-mode` |
| `expected_width_pt` / `expected_height_pt` / `tolerance_pt` | `page-size`, `trim` |

## Report JSON

Required top-level fields: `pass`, `profile`, `errors`, `warnings`, `fixups_available`.

Every finding in `errors[]` / `warnings[]` **must** include:

| Field | Notes |
|-------|-------|
| `page` | 1-based |
| `type` | kebab-case machine id |
| `severity` | `error` \| `warning` \| `info` |
| `message` | human text for the dock panel |
| `bbox` | `[x0, y0, x1, y1]` in PDF user space (points), media-box lower-left origin |

`bbox` is mandatory from day one so Phase 2 overlays (`IDocumentDrawInterface`) do not need a second geometry pass. Page-level issues should use the relevant page box (typically MediaBox or TrimBox).

`object_id` is optional (string or null). `fixups_available[]` entries need `id`, `safe`, `description`.

## Intended CLI (MIC-133)

```bash
PdfTool preflight document.pdf --profile frisket-preflight/profiles/frisket-default.json
```

- Exit `0` when `pass` is true; exit `1` when `errors[]` is non-empty.
- stdout: single JSON document validating against `schemas/report.schema.json`.
- Profiles: **JSON** at runtime today (`frisket-default.json` mirrors the YAML). YAML authoring is fine; convert or add a loader later.
- Implemented checks: **bleed**, **trim**, and **page-size**, all from page boxes (MIC-134). `trim` and `page-size` are **job-spec dependent** — each is skipped unless its profile check entry supplies both `expected_width_pt` and `expected_height_pt` (compared strictly, orientation-sensitive, within `tolerance_pt`). The generic `frisket-default.json` leaves them unset, so those two checks are no-ops there until a job-specific profile sets a size. Remaining checks (fonts, color mode, image DPI) land in later issues.

Other PdfTool commands accept `--console-format json` via `PDFOutputFormatter` (tree JSON, not the preflight report schema).

## Versioning

Bump `schema_version` only together with engine + plugin releases so the JSON contract stays in sync (see packaging notes in the hybrid sidecar plan).

## Golden corpus & CI (MIC-132)

A golden corpus of fixture PDFs is run against `PdfTool preflight` in CI via a QtTest
executable, `UnitTestsPreflightCorpus` (`UnitTests/tst_preflightcorpus.cpp`), registered
with `ctest` in `UnitTests/CMakeLists.txt`. `.github/workflows/ci.yml` runs
`ctest --output-on-failure` after the build, so a regression in any check's pass/fail
outcome or report output fails the build.

### Layout

| Path | Purpose |
|------|---------|
| `testdata/manifest.schema.json` | JSON Schema for corpus manifest entries |
| `testdata/fixtures/manifest.json` | Corpus manifest: fixture → profile → expected outcome |
| `testdata/fixtures/*.pdf` | Fixture PDFs (hand-built or generated; **not** client/job PDFs) |
| `testdata/snapshots/<id>.json` | Golden report JSON per fixture, normalized (`engine_version`/`pdf` stripped) |
| `tools/generate_fixtures.cpp` | Deterministically (re)generates the `bleed-*` fixtures via `pdf::PDFDocumentBuilder` |

### Manifest entries

```json
{
  "id": "bleed-adequate",
  "pdf": "bleed-adequate.pdf",
  "profile": "profiles/frisket-default.json",
  "expect": { "pass": true, "check_ids": [] },
  "source": "generated",
  "notes": "short description of what the fixture exercises"
}
```

`expect.check_ids` lists the `check_id`s that must appear in `errors[]`/`warnings[]`; leave
empty for a clean pass. `UnitTestsPreflightCorpus` reads this manifest at test time — no
code changes are needed to add a fixture, only a new PDF + manifest entry (+ an initial
snapshot, below).

### Adding a fixture

1. Get a PDF: either extend `tools/generate_fixtures.cpp` (preferred for parametric cases
   like bleed/trim/DPI amounts) or hand-build one (e.g. for font-embedding or color-mode
   cases that need real content). Put it in `testdata/fixtures/`. No client/job PDFs.
2. Add a `manifest.json` entry with the expected `pass` and `check_ids`.
3. Create its snapshot once locally: `FRISKET_UPDATE_SNAPSHOTS=1 ctest -R UnitTestsPreflightCorpus`,
   then review and commit the new `testdata/snapshots/<id>.json`.

Until a fixture's PDF exists on disk, `UnitTestsPreflightCorpus` skips (not fails) its rows,
so an in-progress manifest entry doesn't redden CI.

### Regenerating the seeded fixtures

```bash
cmake --build build --target FrisketGenerateFixtures
./build/usr/bin/FrisketGenerateFixtures frisket-preflight/testdata/fixtures
FRISKET_UPDATE_SNAPSHOTS=1 ctest --test-dir build -R UnitTestsPreflightCorpus
```

Review the resulting diff before committing — a snapshot change should always be traceable
to an intentional check-behavior change, not an accident.

### Snapshot vs. manifest checks

The manifest check (`preflightMatchesManifest`) is the correctness gate: does this fixture
still pass/fail the way it's supposed to. The snapshot check (`preflightMatchesSnapshot`) is
the regression gate: did anything about the report's *content* (message text, bbox, severity,
finding order) change, even if pass/fail didn't. Both run for every corpus entry.

### Public PDF/A corpora and the hand-built check matrix

This directory currently seeds only the `bleed` check (the only one `PdfTool preflight`
implements so far). The full hand-built fixture matrix (trim, page-size, image-resolution,
color-mode, embedded-fonts) is tracked separately in MIC-145, and a pinned subset of public
PDF/A corpora (veraPDF-corpus, Isartor, BFO) in MIC-146 — both add entries to this same
manifest and drop fixtures into `testdata/fixtures/` without needing to change the runner.
