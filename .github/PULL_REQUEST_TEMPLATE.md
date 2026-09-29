## What / Why

<!-- What changed, and why — the motivation, not a restatement of the diff. -->

## Spec / ADR

<!-- Link the spec this implements (apps/server/specs/<module>/<feature>.md)
     and/or the ADR for any architectural decision this PR makes. -->

## Checklist (apps/server)

- [ ] `task check` passes locally (gen:check, lint, arch, vuln, test)
- [ ] No hand-edited generated code (`gen/`, `contracts` output) — regenerate instead
- [ ] No new dependency without a documented reason (ADR for anything architectural)
- [ ] A new command handler that should be safe to retry has an idempotency-key test
- [ ] A new integration event has an outbox-write test and an inbox-dedup test
- [ ] Migrations pass `task lint:migrations` (no unguarded table lock on a large table)

## Checklist (other apps)

- [ ] The relevant app's CI is green
- [ ] No unrelated changes bundled from other apps
