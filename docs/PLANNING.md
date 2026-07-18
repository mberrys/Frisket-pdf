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

## Worked example: MIC-151

MIC-151 (two-tier bleed preflight) follows the M0-before-code process:

1. **Plan doc:** [PREFLIGHT_TIERED_BLEED_PLAN.md](PREFLIGHT_TIERED_BLEED_PLAN.md) locks the
   Tier-1/Tier-2 flow, class names (`PDFDocumentSession`, `PreflightEngine`,
   `PDFBleedMarginProbe`), profile params, and report extensions before any code.
2. **Related issues:** MIC-154 (PreflightEngine refactor) and MIC-157 (PDFDocumentSession) are
   scaffolding issues; MIC-158 (content-bleed check) and MIC-159 (strip probe) depend on them.
   All were blocked until M0 locked the shared shapes.
3. **Surface order:** PdfTool preflight first (thin driver), then Editor plugin (QProcess),
   then PageMaster batch. Same order as MIRROR_BLEED_PLAN.md.
4. **New trap to lock early:** Tier-1 box checks and Tier-2 content checks must share the
   same reference-box fallback (TrimBox → CropBox → MediaBox). Don't let them diverge — if
   they pick different boxes, the "skip Tier-2 when Tier-1 passes" gate is unsound.

## What not to put in AGENTS.md

Keep DPI, sample-pixel defaults, corner policies, CLI flag lists, and issue IDs in the feature plan / Linear — not in agent-wide rules.
