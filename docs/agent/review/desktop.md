# AI review prompt: apps/desktop

Review the pull request diff under `apps/desktop/`. Report findings only; do not push fixes.

## Read first

1. `apps/desktop/AGENTS.md` (situational rules), `apps/desktop/CONTRIBUTING.md` and `apps/desktop/CODING_STANDARDS.md`
2. `docs/content/library-first/desktop.md` (approved stack, explicit rejections)
3. `docs/content/decisions/desktop.md` and `apps/desktop/docs/adr/` (recorded decisions; do not accept a contradicting change)
4. `docs/agent/conventions/desktop.md` (naming)

## Also check

- C++ stays within the subset enforced by `apps/desktop/scripts/check-cxx-subset.sh`; formatting matches `apps/desktop/scripts/check-format.sh`; QML passes `apps/desktop/scripts/check-qml-lint.sh`.
- Feature boundaries hold (`apps/desktop/scripts/check-feature-boundaries.sh`); naming holds (`apps/desktop/scripts/check-naming.sh`).
- Banned: Conan, hand-mirrored DTOs, committed generated code, RPCs missing from the contract methods list, shipping `QT_QML_DEBUG`.
- New vcpkg dependencies are in `apps/desktop/vcpkg.json` and in `docs/content/library-first/desktop.md`; license impact is considered (`apps/desktop/compliance`).
- Protobuf freshness (`apps/desktop/scripts/check-proto-freshness.sh`) is preserved; secrets go through the keychain wrapper, never plain settings.
- Generated palettes and QML tokens (`AedstTokens.qml`, `Aedst{Light,Dark}Palette.*`) are not hand-edited.

Post findings as a PR review. If nothing applies, approve with no comments.
