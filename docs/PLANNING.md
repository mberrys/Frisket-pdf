# Planning related multi-surface features

Short process notes for Frisket work that spans Core, PdfTool, PageMaster, and/or Editor — especially when several Linear issues share one API.

## When this applies

- Two issues describe variants of the same fixup (e.g. MIC-121 mirror bleed and MIC-122 pixel-repeat/stretch).
- A feature needs the same Core `apply()` from PdfTool (validation) and PageMaster (batch), with optional Editor confirm later.
- Naming or box policy is still fluid and a rename would touch docs, Linear, and code.

## M0 before code

1. Write or amend a plan under `docs/` (example: [MIRROR_BLEED_PLAN.md](MIRROR_BLEED_PLAN.md)).
2. Lock: type/command names, settings defaults, box nesting, content-stream order, PageMaster export order, PdfTool flag semantics.
3. Mirror those locks into Linear issue descriptions; set **blocked by** when a later mode depends on shared scaffolding.
4. Only then implement shared Core + first mode (usually PdfTool first).

Cost of skipping M0: doc/Linear churn and half-built names (`PDFMirrorBleed` vs `PDFBleedFixup`, `mirror-bleed` vs `add-bleed --mode`).

## Sequencing that worked in review

| Step | Why |
|------|-----|
| Core `apply()` + first mode | Proves box expand → paint → PlaceBefore without UI |
| PdfTool command | Non-interactive harness (`--report` / `--force` / `--dry-run`) |
| Unit tests for pure math | Rect/skip/strip builders; full render fixtures optional |
| PageMaster JSON + export hook | Batch path after geometry |
| Editor confirm | Optional; not the batch home |

## Architecture traps to lock early

See [AGENTS.md](../AGENTS.md) § Page boxes and prepress fixups. In short:

- Geometry box flags ≠ artwork generation
- Expand MediaBox before `PDFPageContentStreamBuilder::begin()`
- PageMaster: geometry before content fixups
- Prefer one API + mode enum over parallel pipelines

## What not to put in AGENTS.md

Keep DPI, sample-pixel defaults, corner policies, CLI flag lists, and issue IDs in the feature plan / Linear — not in agent-wide rules.
