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
- Implemented checks grow in MIC-134+; scaffold runs **bleed** from page boxes.

Other PdfTool commands accept `--console-format json` via `PDFOutputFormatter` (tree JSON, not the preflight report schema).

## Versioning

Bump `schema_version` only together with engine + plugin releases so the JSON contract stays in sync (see packaging notes in the hybrid sidecar plan).
