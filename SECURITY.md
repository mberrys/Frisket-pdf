# Security Policy

## Supported versions

Frisket-PDF is released from the `master` branch of [mberrys/Frisket-pdf](https://github.com/mberrys/Frisket-pdf).

| Version | Supported |
| ------- | --------- |
| 1.6.x (current `PDF4QT_VERSION`) | Yes — security fixes land here |
| Older fork tags / unreleased branches | Best effort only |

Upstream PDF4QT releases are not covered by this policy; report upstream issues to [JakubMelka/PDF4QT](https://github.com/JakubMelka/PDF4QT) when they are not Frisket-specific.

## Reporting a vulnerability

Please report security issues privately:

1. Open a **private** vulnerability report on GitHub for `mberrys/Frisket-pdf` (Security advisories), **or**
2. Email the maintainer listed on the GitHub profile for this fork.

Include:

- Affected binary/surface (PdfTool, Editor, PageMaster, library)
- Frisket version / commit hash
- Minimal PDF or steps to reproduce
- Crash / DoS / info-disclosure impact

You should receive an acknowledgement within **7 days**. We aim to ship a fix or mitigation guidance within **30 days** for confirmed high-severity issues (crash/DoS on untrusted PDFs, path traversal, unsafe writes).

Please do **not** open a public GitHub issue for unfixed vulnerabilities.

## Scope notes

Highest-risk surfaces for this project:

- PDF parsers and stream filters (`Pdf4QtLibCore`)
- Image codecs (JBIG2, CCITT, DCT)
- Attachment / launch / URI handlers
- Atomic write / export paths (PageMaster, PdfTool)

Fuzz harnesses live under `Fuzz/` and `.github/workflows/fuzz.yml`. Crash reporting may be sent via Sentry when `SENTRY_DSN` / `PDF4QT_ENABLE_SENTRY` is configured — treat that as operational telemetry, not a substitute for private disclosure. Sentry is configured with `send_default_pii` disabled; see `docs/PRODUCTION_RUNBOOK.md` for opt-in guidance.

For V1 launch readiness and operational procedures, see `docs/V1_RELEASE_READINESS.md` and `docs/PRODUCTION_RUNBOOK.md`.
