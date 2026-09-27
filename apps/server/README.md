# aedst-server

The AEDST backend: a modular monolith (see [../../docs/architecture.md](../../docs/architecture.md)
for the full architecture and [../../docs/modules.md](../../docs/modules.md) for
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
curl -s http://localhost:8080/aedst.users.v1.UsersService/RegisterUser \
  -H 'Content-Type: application/json' \
  -d '{"email":"you@example.com","password":"password123","deviceId":"cli"}'
```

That call: creates a credential (auth module), creates the identity record
(users module), issues the first session/token pair, and publishes
`UserRegistered` — which the profile module consumes to create a default
profile (see [../../docs/events.md](../../docs/events.md)).

## Common tasks

| Command | What it does |
|---|---|
| `make up` / `make down` | start/stop local Postgres, Redis, NATS |
| `make migrate-up` / `make migrate-down` | apply/roll back migrations, per module schema |
| `make generate` | regenerate proto (buf) and SQL (sqlc) code — commit the diff |
| `make lint` | format check, vet, golangci-lint, buf lint, architecture boundary tests |
| `make test` | unit tests (no external services needed) |
| `make integration-test` | tests against real Postgres/Redis/NATS via Testcontainers |
| `make security` | `govulncheck` |
| `make build` | compile `bin/aedst-server` |

## Adding a module

1. Add a row + dependency edges to [../../docs/modules.md](../../docs/modules.md).
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
