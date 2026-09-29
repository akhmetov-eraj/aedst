# aedst-server

The AEDST backend: a modular monolith (see [../../docs/content/architecture.md](../../docs/content/architecture.md)
for the full architecture and [../../docs/content/modules.md](../../docs/content/modules.md) for
the module list and dependency graph).

## Prerequisites

- Go 1.27.1
- Docker (for local Postgres/Redis/NATS)
- [buf](https://buf.build/docs/installation), [sqlc](https://docs.sqlc.dev/en/latest/overview/install.html),
  [goose](https://github.com/pressly/goose) — only needed if you change
  `api/proto/`, `sql/`, or `migrations/`; not needed to just run the server.

## Quickstart

```bash
cp .env.example .env   # edit AEDST_JWT_SIGNING_KEY etc. for anything beyond a laptop
make dev                # starts Postgres/Redis/NATS, runs migrations, runs the server
```

The server listens on `:8080` (ConnectRPC — Connect/JSON, gRPC and gRPC-Web all
work against it) and exposes Prometheus metrics on `:9090/metrics`.

Try it against the running server (Connect's protocol accepts plain JSON over
HTTP/1.1, so `curl` works with no gRPC tooling):

```bash
curl -s http://localhost:8080/aedst.auth.v1.AuthService/Register \
  -H 'Content-Type: application/json' \
  -d '{"email":"you@example.com","password":"password123","deviceId":"cli"}'
```

That call creates the credential and identity record (both owned by the
auth module — see [ADR-0007](../../docs/content/decisions/0007-merge-users-into-auth.md))
and issues the first session/token pair. Try the reference module next:

```bash
curl -s http://localhost:8080/aedst.tasks.v1.TasksService/CreateTask \
  -H 'Content-Type: application/json' \
  -H "Authorization: Bearer $(echo '<paste accessToken from the Register response>')" \
  -d '{"title":"Try the reference module","priority":"TASK_PRIORITY_MEDIUM"}'
```

See [`api/bruno/`](api/bruno/README.md) for the full request set instead of
copy-pasting tokens by hand.

For anything past a one-off curl — the full request set, with auth-token
chaining and negative tests for the 401/403 paths — see
[`api/bruno/`](api/bruno/README.md): a [Bruno](https://www.usebruno.com/)
collection, this project's Swagger-UI equivalent for a ConnectRPC API (no
generated try-it-out page exists for Connect the way OpenAPI/Swagger has
one, so this is a real, runnable, git-committed collection instead).

## Common tasks

| Command | What it does |
|---|---|
| `make up` / `make down` | start/stop local Postgres, Redis, NATS |
| `make migrate-up` / `make migrate-down` | apply/roll back migrations, per module schema |
| `make generate` | regenerate proto (buf) and SQL (sqlc) code — commit the diff |
| `make lint` | format check, vet, golangci-lint, buf lint/format, architecture boundary tests |
| `make breaking` | fail if this branch's proto changes break compatibility against `main` |
| `make test` | unit tests (no external services needed) |
| `make integration-test` | tests against real Postgres/Redis/NATS via Testcontainers |
| `make security` | `govulncheck` |
| `make build` | compile `bin/aedst-server` |

## API governance (Buf)

`api/proto/` is the single source of truth every client generates from
(`apps/web`, `apps/apple`, and this server's own Go stubs — see
`buf.gen.yaml` in each). `buf.yaml` pins the lint ruleset (`STANDARD`) and
breaking-change detection (`FILE`-level compatibility). `make lint` runs
`buf lint` + `buf format -d --exit-code` (fails on unformatted `.proto`);
`make breaking` checks the current branch against `main` — CI runs the
equivalent of both on every PR touching `apps/server/**`
(`.github/workflows/server-ci.yml`), plus a check that `gen/` isn't stale
relative to `api/proto/`.

## Adding a module

1. Add a row + dependency edges to [../../docs/content/modules.md](../../docs/content/modules.md).
2. Scaffold `internal/modules/<name>/{domain,application,infrastructure,repository,transport,events,api}`.
3. Add `api/proto/aedst/<name>/v1/<name>.proto` (+ `events.proto` if it publishes events) and `make generate`.
4. Add `migrations/<name>/` (its own schema) and `sql/<name>/` + an `sqlc.yaml` entry.
5. Wire it in `cmd/server/main.go`.
6. `go test ./architecturetest/...` — this fails if the new module's imports
   don't match what you declared in `modules.md`.

## Verifying this environment

`go build ./...`, `go vet ./...`, `golangci-lint run ./...`, `govulncheck ./...`
and `go test ./...` (architecture boundary tests + unit tests) all run without
Docker. `make dev` / `make integration-test` need Docker for Postgres/Redis/NATS.
