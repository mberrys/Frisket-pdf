# Frisket code handoff workflow

Move small generated artifacts (fixtures, manifests, scripts) from Notion to the GitHub repo without uploading the whole repo to a sandbox.

**Notion source:** [Frisket Code Handoff Workflow](https://app.notion.com/p/berrymichael/Frisket-Code-Handoff-Workflow-9e709da844e84e048008f0e257cf2d0c)

## Goal

Stage hand-built or generated files in Notion, pull them into the repo with review, and commit on a feature branch.

## Repo target

| Item | Value |
|------|-------|
| **Repo** | [mberrys/Frisket-pdf](https://github.com/mberrys/Frisket-pdf) |
| **Default branch for Phase 1 fixtures** | `phase-1-check-fixtures` (or a `cr/*-d2b3` feature branch off it) |
| **Fixture path** | `frisket-preflight/testdata/fixtures/` |

## Decided design

- **Pull agent:** Cursor (manual run by a human).
- **Trigger:** Manual — not automated from Notion.
- **Diff behavior:** Agent pulls files, runs `git diff`, writes the diff summary back to the Notion staging page Notes, and pauses for confirmation before pushing.
- **Status tracking:** Each handoff is a row in the Notion **Frisket Code Handoffs** database with Status = Draft / Ready / Pulled / Diff Summary / Pushed / Failed.

## Staging page contents

Each handoff page should contain:

- Files as attachments (PDFs, scripts, manifests)
- `manifest.json` mapping each file → expected pass/fail + check ids
- Target repo path (e.g. `frisket-preflight/testdata/fixtures/`)
- Branch name for the commit
- Linear issue link
- Status property

## Cursor agent steps

1. Human runs Cursor manually against the handoff page.
2. Cursor reads the Notion handoff row and downloads file attachments.
3. Cursor checks out the repo and places files at the target path (using **repo naming conventions** — see below).
4. Cursor runs `git diff` and writes the diff summary to the Notion page Notes / body.
5. Cursor sets Status = **Diff Summary** and pauses for human confirmation.
6. Human confirms → Cursor commits and pushes to the branch.
7. Cursor updates Status = **Pushed**, records the commit SHA and PR link, and clears any failure notes.

## MIC-145 — Golden PDF fixtures (landed)

**Linear:** [MIC-145](https://linear.app/mbx2/issue/MIC-145/p1-hand-built-golden-pdf-fixtures-for-frisket-custom-checks)  
**Notion staging:** [MIC-145 — Golden PDF Fixtures](https://app.notion.com/p/40a7617c682642eb869818c42d898e39)

Status in repo: **complete** (PR #11 fixtures + manifest; PR #13 trim/page-size snapshots). Regeneration: `frisket-preflight/tools/generate_fixtures.py` and `tools/generate_fixtures.cpp`. Full table and `pending` rules: [frisket-preflight/README.md](../frisket-preflight/README.md) § Hand-built custom-check fixtures.

Notion staging used numbered filenames; the repo uses semantic names:

| Notion attachment | Repo file | Check(s) |
|-------------------|-----------|----------|
| `01_bleed_insufficient.pdf` | `bleed-missing.pdf` | `bleed` (fail) |
| `02_bleed_adequate.pdf` | `bleed-adequate.pdf` | `bleed` (pass) |
| `03_trim_mismatch.pdf` | `trim-pagesize-mismatch.pdf` | `trim`, `page-size` (fail) |
| `04_trim_ok.pdf` | `trim-pagesize-ok.pdf` | `trim`, `page-size` (pass) |
| `05_image_low_dpi.pdf` | `image-dpi-low.pdf` | `image-resolution` (warning; pending) |
| `06_image_high_dpi.pdf` | `image-dpi-ok.pdf` | `image-resolution` (pass; pending) |
| `07_color_rgb.pdf` | `color-rgb.pdf` | `color-mode` (fail; pending) |
| `08_color_grayscale.pdf` | `color-cmyk.pdf` | `color-mode` (pass; pending — repo uses CMYK, not grayscale) |
| `09_font_not_embedded.pdf` | `font-not-embedded.pdf` | `embedded-fonts` (fail; pending) |
| `10_font_embedded.pdf` | `font-embedded.pdf` | `embedded-fonts` (pass; pending) |

Six fixtures remain `"pending": true` in `manifest.json` until MIC-148, MIC-149, and MIC-150 implement the engine checks. That is intentional — MIC-145 scope is fixtures + manifest only.

## MIC-140 — Packaging and licensing

Decision record and release layout: [PACKAGING_LICENSING.md](PACKAGING_LICENSING.md)  
**Notion source:** [MIC-140 — Packaging & Licensing Review](https://app.notion.com/p/9bdbe383233d44cd88b7916d9aa4ce6d)

## Related docs

- [REPO_MAP.md](REPO_MAP.md) — fork layout and upstream sync
- [PLANNING.md](PLANNING.md) — multi-surface feature planning
- [frisket-preflight/README.md](../frisket-preflight/README.md) — golden corpus and fixture regeneration
