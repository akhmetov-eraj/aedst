# aedst-server

The AEDST backend: a modular monolith (see [../../docs/content/architecture.md](../../docs/content/architecture.md)
for the full architecture and [../../docs/content/modules.md](../../docs/content/modules.md) for
the module list and dependency graph).

## Prerequisites

- Go 1.27.1
- Docker (for local Postgres + Valkey, and Testcontainers-based integration tests)
- [buf](https://buf.build/docs/installation), [sqlc](https://docs.sqlc.dev/en/latest/overview/install.html),
  [goose](https://github.com/pressly/goose) — only needed if you change
  `api/proto/`, `sql/`, or `migrations/`; not needed to just run the server.

## Quickstart

```bash
cp .env.example .env   # edit AEDST_JWT_SIGNING_KEY etc. for anything beyond a laptop
make dev                # starts Postgres + Valkey, runs migrations, runs the server
```

The server listens on `:8080` (ConnectRPC — Connect/JSON, gRPC and gRPC-Web all
work against it). Health: `GET /livez` (process up), `GET /readyz` (Postgres required; Valkey optional — reported as `degraded`, never 503) and standard gRPC health (`grpc.health.v1`).
Draining reports 503. Telemetry is OpenTelemetry (OTLP) — see
[Observability](#observability); a Prometheus scrape endpoint is opt-in via
`AEDST_METRICS_ADDR`.

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
| `make up` / `make down` | start/stop local Postgres + Valkey (`task up PROFILES="--profile observability --profile idp"` adds Grafana otel-lgtm and Keycloak) |
| `make migrate-up` / `make migrate-down` | apply/roll back migrations, per module schema |
| `make generate` | regenerate proto (buf) and SQL (sqlc) code — commit the diff |
| `make lint` | format check, vet, golangci-lint, buf lint/format, architecture boundary tests |
| `make breaking` | fail if this branch's proto changes break compatibility against `main` |
| `make test` | unit tests (no external services needed) |
| `make integration-test` | tests against a real Postgres via Testcontainers (advisory locks, outbox, session CAS, repositories) |
| `task lint:migrations` | squawk migration safety lint (Up sections; PATH, npx or Docker) |
| `task lint` | also runs `sqlc vet` |
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
Docker. `make dev` / `make integration-test` need Docker for Postgres/Valkey.

## Stack at a glance

| Concern | Choice | Notes |
|---|---|---|
| Router / RPC | Chi + ConnectRPC (Protobuf, Buf, Protovalidate) | Chi kept on purpose: template project, familiar middleware ecosystem (CORS, timeout, client-IP) |
| Data | PostgreSQL 18, pgx, sqlc, Goose | `sqlc vet` + squawk in the lint gate |
| Async | **Postgres outbox + inbox + River** | one mechanism family, no broker; `internal/platform/messaging.Publisher` is the seam if a broker is ever justified |
| Locks | PostgreSQL advisory locks (`internal/platform/lock`) | no Redlock/RedSync |
| Cache / rate limits | Valkey via go-redis + redis_rate (`internal/platform/valkey`) | `AEDST_VALKEY_*` |
| Auth | OIDC resource server (any IdP) + optional first-party module | [ADR-0001](docs/adr/0001-identity-provider.md) |
| Observability | OpenTelemetry (traces, metrics, logs) over OTLP; optional `/metrics` | any backend |

## Authentication

Two verifiers behind one interface (`platformauth.Verifier`):

- **External IdP (recommended for new projects):** set `AEDST_OIDC_ISSUER`,
  `AEDST_OIDC_AUDIENCE` (and `AEDST_OIDC_ROLES_CLAIM` if roles are not in
  `roles`). Discovery, JWKS caching and rotation are automatic. `docker compose
  --profile idp up` starts a pre-configured Keycloak; see
  [ADR-0001](docs/adr/0001-identity-provider.md) for the provider analysis
  (Keycloak vs Zitadel vs Ory) and a curl walkthrough.
- **First-party (`AEDST_AUTH_FIRST_PARTY_ENABLED=true`, default):** the
  existing `AuthService` for the four clients: argon2id, atomic refresh
  rotation with reuse detection, per-IP and per-account rate limits. Turn it off
  when an IdP is the system of record.

## Observability

The server speaks **OpenTelemetry only**; the backend is a deployment choice.
Set the standard `OTEL_EXPORTER_OTLP_*` variables and traces, metrics and logs
(slog, correlated by `trace_id`) are exported over OTLP. Unset = nothing is
exported.

```bash
# local all-in-one backend (Grafana + Loki + Tempo + Prometheus/Mimir + OTLP receiver)
task up PROFILES="--profile observability"
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318   # Grafana: http://localhost:3001
```

Grafana Cloud, SigNoz and HyperDX work the same way with their OTLP endpoint
and `OTEL_EXPORTER_OTLP_HEADERS`. `AEDST_METRICS_ADDR=:9090` additionally serves
`/metrics` for Prometheus scraping from the same OpenTelemetry meter.

## Migrations

`goose` per module schema, each with its own version table
(`goose_<module>`, so two modules can both have a `00001`). Every migration is
linted with squawk before merge (`task lint:migrations`); mark migrations that
use `CREATE INDEX CONCURRENTLY` with `-- +goose NO TRANSACTION`.
Upgrading an existing dev database that was migrated with the old shared
`goose_db_version` table: `docker compose down -v` and re-migrate.

## Configuration changes from the previous stack

| Old | New |
|---|---|
| `AEDST_REDIS_ADDR/PASSWORD/DB` | `AEDST_VALKEY_ADDR/PASSWORD/DB` (same protocol) |
| `AEDST_NATS_URL` | removed |
| `AEDST_OTLP_ENDPOINT` (gRPC, insecure) | standard `OTEL_EXPORTER_OTLP_ENDPOINT` / `_HEADERS` / `_PROTOCOL` |
| `AEDST_METRICS_ADDR` default `:9090` | default empty (opt-in) |
| `AEDST_JWT_SIGNING_KEY` any length | >= 32 bytes; dev placeholder rejected outside development |
| — | `AEDST_OIDC_*`, `AEDST_AUTH_FIRST_PARTY_ENABLED`, `AEDST_TRUSTED_PROXY_HOPS`, `AEDST_AUTH_RATE_LIMIT_PER_MINUTE`, `AEDST_LOGIN_RATE_LIMIT_PER_MINUTE`, `AEDST_SHUTDOWN_DRAIN_SECONDS`, `AEDST_DATABASE_*` pool/timeouts |
