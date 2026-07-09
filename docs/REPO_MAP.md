# Repo map

Frisket-pdf repository layout, fork relationship, and upstream-tracking policy.

## Repositories

| Role | Repository | Default branch |
|------|------------|----------------|
| Fork (canonical for Frisket work) | [mberrys/Frisket-pdf](https://github.com/mberrys/Frisket-pdf) | `master` |
| Upstream (read-only source) | [JakubMelka/PDF4QT](https://github.com/JakubMelka/PDF4QT) | `master` |

Frisket-pdf is a GitHub fork of PDF4QT (MIT). Upstream is the source of PDF engine, Qt apps, CLI, and build tooling; Frisket owns branding, licensing text, and downstream product changes.

## Upstream-tracking policy

### Policy: on-demand GitHub Sync fork

**Decision:** Pull upstream changes only when you explicitly sync — via GitHub’s **Sync fork** UI or `gh repo sync`. No automatic or scheduled upstream merges.

| Do | Don’t |
|----|-------|
| Use **Sync fork → Update branch** on GitHub when you want upstream changes | Auto-sync on a schedule or in CI |
| Merge upstream `master` into Frisket `master` (GitHub default) | Rebase Frisket commits unless you deliberately choose that on GitHub |
| Keep Frisket-only commits on top of upstream | Force-push `master` to discard fork history |
| Re-apply Frisket branding after sync if upstream touched the same files | Push Frisket branding/licensing back to upstream |

**Rationale:** Early fork, minimal divergence (2 Frisket-only commits today), and you want control over when upstream lands — especially for `README.md` / license / branding.

### Current fork state (2026-07-09)

| Metric | Value |
|--------|-------|
| Upstream HEAD | `a3df6db` — Issue #396: Inherit Zoom for Hyperlink to this PDF |
| Frisket HEAD | `bb3660a` — Updated LICENSE |
| Commits ahead of upstream | 2 (`803b035` README, `bb3660a` LICENSE) |
| Commits behind upstream | 0 (upstream has not moved since fork point) |
| Last upstream release | v1.6.0.0 (`23f3829`, Jun 2026) |

### Fork-only changes (preserve on sync)

These are intentional Frisket deltas; don’t drop them during conflict resolution:

- **README.md** — Frisket copyright, acknowledgements, CI badge (`mberrys/Frisket-pdf`)
- **LICENSE** — Frisket licensing posture (upstream MIT file removed/modified in `bb3660a`)

Everything else should track upstream unless there is an explicit Frisket feature branch or product decision not to merge.

### When to sync

- Upstream ships a release you care about (check [PDF4QT releases](https://github.com/JakubMelka/PDF4QT/releases))
- You need a specific upstream bugfix or feature
- Before a large Frisket feature branch, to reduce future merge pain

No fixed cadence required.

### How to sync

**On GitHub (preferred):**

1. Open [mberrys/Frisket-pdf](https://github.com/mberrys/Frisket-pdf)
2. **Sync fork** → review commits → **Update branch**
3. If GitHub reports conflicts, resolve via the offered PR or locally (see below)

**CLI equivalent:**

```bash
gh repo sync mberrys/Frisket-pdf -b master
```

**Local after GitHub sync:**

```bash
git fetch origin
git pull origin master
```

**Optional local upstream remote** (for inspection, not required if you only use GitHub Sync):

```bash
git remote add upstream https://github.com/JakubMelka/PDF4QT.git   # once
git fetch upstream
git log master..upstream/master --oneline   # what's new upstream
```

### Conflict handling

Likely conflict files: `README.md`, `LICENSE`, `.github/**`, `CMakeLists.txt`, `RELEASES.txt`.

- Prefer keeping Frisket branding/licensing in `README.md` / `LICENSE`
- Take upstream code fixes everywhere else
- Run targeted builds/tests after merge (see [AGENTS.md](../AGENTS.md))
- Record the sync in `RELEASES.txt` or a short merge commit message, e.g. `Sync upstream PDF4QT @ <sha>`

### What we do not do

- No PRs from Frisket → upstream unless you explicitly decide to contribute back
- No renaming/removing upstream copyright headers in source files (MIT attribution stays)
- No wholesale replacement of PDF4QT module layout without a documented Frisket architecture decision

### Agent / contributor note

See [AGENTS.md](../AGENTS.md) for build and edit rules. For upstream work: do not run `git fetch upstream` or merge upstream unless the user asks in that session.
