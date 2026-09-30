# Contributing — AEDST Desktop

Read first: root [AGENTS.md](../../AGENTS.md), [CONVENTIONS.md](../../CONVENTIONS.md),
[library-first.md](../../docs/content/library-first.md),
[decisions.md](../../docs/content/decisions.md), then
[ARCHITECTURE.md](ARCHITECTURE.md), [CODING_STANDARDS.md](CODING_STANDARDS.md),
[TESTING.md](TESTING.md), [SECURITY.md](SECURITY.md).

## Workflow

1. Branch from `main`: `desktop/<short-topic>`. One concern per PR.
2. Local gate before pushing: `scripts/check-format.sh`,
   `scripts/check-feature-boundaries.sh`, `scripts/check-naming.sh`,
   `scripts/check-qml-lint.sh`, then configure/build/test a preset
   (`scripts/verify-desktop-local.sh gcc-debug`).
3. Commit messages: imperative summary, scope prefix (`desktop:`), body explains
   *why*. Reference the ADR/decision when touching one.
4. Open a PR using the checklist below; CI must be green (lint, tests on all
   platforms, sanitizers, coverage, package, install-verify).

## Definition of done

- [ ] No hand-rolled code where a standard-library/Qt/approved-library facility exists.
- [ ] New dependency: allowlist entry **and** `library-first.md` updated in the same PR.
- [ ] New RPC: entry in `Contract/methods.hpp`; proto freshness script passes.
- [ ] Tests added (unit/qttest/qmltest as appropriate) and regression test for fixes.
- [ ] Warnings clean under `AEDST_WARNINGS_AS_ERRORS=ON`; clang-tidy adds no new findings.
- [ ] User-visible strings via `qsTr` and present in all five `.ts` catalogs (en, ru, ar, tg, uz).
- [ ] Accessibility: roles/names, focus order, keyboard path, RTL check.
- [ ] Docs/ADR updated when a decision changes; `PLATFORM_SUPPORT.md` when support changes.
- [ ] No secrets in code, logs, fixtures or screenshots.

## Code review

Reviewers check boundaries (features never link each other), thread affinity,
lifetimes of callbacks, error mapping (`ApiError` → domain `Error`), secret
handling, and that tests fail without the change. Apple and Desktop have not been
built by the authoring environment — never claim "builds" without a CI run.
