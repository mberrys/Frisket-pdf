# Berry Studio — policy brief (agent session context)

Non-negotiable constraints for every session in this repo:

- **Branching:** trunk-based. Never commit on `main`, `master`, or `stable`. Use
  `feature/`, `bugfix/`, or `hotfix/` branches and open a PR.
- **Commits:** Conventional Commits v1.0.0. Signed commits required before push.
- **Secrets:** never stage or write `.env`, keys, tokens, or credentials. Use
  `.env.example` with placeholders only. No `--no-verify`.
- **Data:** no real customer or research personal data in fixtures, seeds, or tests.
- **Storage:** private docs, data, logs, and artifacts live under
  `C:\.dev\<category>\<repo>\`, not in the Git tree. See `C:\.dev\README.md`.
- **Docs-with-code:** user-facing or API doc changes ship in the same PR as the code.
- **Hooks:** client-side gates are fast feedback only; CI is authoritative. Do not
  bypass pre-commit or pre-push checks.
- **Frisket-pdf:** Qt 6.11.1+ C++20 fork; Editor is the plugin host; PdfTool for
  headless automation; PageMaster for batch geometry. Do not run full rebuilds unless asked.

Full hook map: Notion → Policy Enforcement Hooks.
