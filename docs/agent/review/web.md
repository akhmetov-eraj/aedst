# AI review prompt: apps/web

Review the pull request diff under `apps/web/`. Report findings only; do not push fixes.

## Read first

1. `apps/web/AGENTS.md` (situational rules: forms, requests, dates, URL state, UI state, analytics)
2. `docs/content/library-first/web.md` (approved stack, explicit rejections)
3. `docs/content/decisions/web.md` (recorded decisions; do not accept a contradicting change)
4. `docs/agent/conventions/web.md` (naming) and `docs/content/ui-kit.md` (UI elements come only from `apps/web/src/shared/ui/`)

## Also check

- Forms use React Hook Form with the protovalidate resolver; server calls go through the generated Connect client and TanStack Query, with no raw `fetch` in modules.
- No ESLint or Prettier config, no RxJS, no new third-party origin without a CSP update in `apps/web/next.config.ts`.
- No color literals in hand-written UI code (`scripts/check-color-literals.mjs`) and all five locales stay in parity (`scripts/check-i18n-parity.mjs`).
- Generated files (`apps/web/src/shared/api/connect/generated/**`, `packages/design-tokens/dist/**`) are not hand-edited.
- New dependencies are in the knip allowlist (`apps/web/knip.jsonc`) and in `docs/content/library-first/web.md`.

Post findings as a PR review. If nothing applies, approve with no comments.
