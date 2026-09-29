# apps/server — AI agent instructions

Read the root [AGENTS.md](../../AGENTS.md) and [CONVENTIONS.md](../../CONVENTIONS.md) first —
this file only adds what's specific to the Go server.

## Before writing a spec-worthy change

A new feature, a new event, or a new command that mutates state should have
a spec under `specs/<module>/<feature>.md` (copy `specs/_template.md`)
**before** implementation — see `.claude/skills/write-spec` if present, or
just fill the template by hand. A one-line bug fix doesn't need one.

## Module boundary rules (enforced, not just documented)

- A module (`internal/modules/<name>/`) is layered
  `domain → application (ports.go + service.go) → infrastructure → transport → api`.
  `domain` depends on nothing but stdlib and `internal/shared/**`.
- Another module is only ever reached through its `api/` package. Direct
  imports of another module's `domain`/`application`/`infrastructure`/`transport`
  are a build-breaking violation — enforced by both `.golangci.yml`'s
  `depguard` rules and `architecturetest/boundaries_test.go` (`task arch`).
- `internal/platform/**` never imports `internal/modules/**` — modules depend
  on platform, never the reverse.
- Only `internal/platform/messaging` imports the NATS client directly.

## When a change touches state and another system cares

Use the transactional outbox (`internal/platform/outbox`), not a direct
`messaging.Publish` call inside a request handler: write the event row in
the same `database.WithTx` as the domain change, let the module's
`outbox.Forwarder` (registered in `cmd/server/main.go`) publish it. See
`internal/modules/tasks/infrastructure/postgres/postgres.go`'s `Complete`
method for the reference implementation, and `internal/platform/inbox` for
the consuming side.

## When a command should be safe to retry

Wrap it with `internal/platform/idempotency`: extract the key via
`idempotency.KeyFromContext` in the transport handler, pass it down as an
explicit argument (never read context inside `application`/`domain`), and
call `idempotency.Check`/`Save` inside the same transaction as the write —
see `Repository.Create` in the same file for the reference implementation.

## Before opening a PR

Run `task check` (gen:check, lint, arch, vuln, unit tests — the same gate
CI runs). `task check:full` additionally runs Docker-dependent gates
(migration linting, integration tests) locally.
