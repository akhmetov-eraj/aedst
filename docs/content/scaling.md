# Scaling guide

[ADR-0001](/decisions/0001-modular-monolith) says a module is extracted "when
there is a measured, concrete reason". This page is what makes that sentence
operable: what to measure, at which numbers to act, how to produce the numbers,
and what is known not to scale yet.

## The scaling model in one paragraph

`aedst-server` is stateless (JWT auth, no in-process sessions, idempotent
commands, config from the environment), so the first lever is **more replicas
behind the edge**. The single Postgres primary is the shared bottleneck, so the
second lever is **not spending it carelessly**: keyset pagination, indexes led
by `user_id`, one INSERT per event, bounded tables, separate connection pools.
Extraction of a module is the third lever and is driven by the triggers below.
Realtime and media are the exception: they are separate deployables from the
start ([ADR-0008](/decisions/0008-realtime-and-media-extraction)).

## What to measure

Everything below is exported via OTLP (and on `/metrics` when the optional Prometheus endpoint is enabled).

| Signal | Metric | Healthy | Act when |
|---|---|---|---|
| Request latency | `http.server.*` / `rpc.server.*` (otelconnect, otelhttp) | p95 < 200 ms on the read path | p95 above SLO for 10 min |
| API pool saturation | `aedst_db_pool_connections{pool="api",state="acquired"}` vs `state="max"` | acquired < 80 % of max | at max for > 1 min, or `aedst_db_pool_empty_acquire_total` rising |
| Jobs pool saturation | same, `pool="jobs"` | idle connections available | at max for > 5 min → event/job backlog is coming |
| Pool wait | `aedst_db_pool_acquire_wait_seconds_total` (rate) | ~0 | rate > 0.1 s/s |
| Event lag | `aedst_outbox_oldest_pending_age_seconds{module}` | < 2 s | > 30 s for 5 min |
| Event backlog | `aedst_outbox_pending{module}` | ~0 | grows monotonically |
| Poisoned events | `aedst_outbox_poisoned_total{module}` | 0 | **any** increase |
| Go runtime | `go_*` / `runtime.*` | steady heap | heap or goroutines climb without bound |
| Database | `pg_stat_activity`, `pg_stat_statements`, replication lag | primary CPU < 60 % | see triggers |

## Extraction and split triggers

Extraction is mechanical ([architecture.md](/architecture#extraction-path)), but
it costs operational complexity, so it needs a measured cause:

| Symptom (sustained, after tuning) | Meaning | Response |
|---|---|---|
| One module's queries dominate `pg_stat_statements` and primary CPU > 60 % | Data-plane contention | Read replica for that module's reads → then database-per-module (schema-per-module already prepares it) |
| One module's handlers dominate CPU/memory on every replica | Compute-plane contention | Extract it to its own `cmd/` + Deployment; scale independently |
| A module needs a different failure domain (a slow dependency must not hold request goroutines) | Blast radius | Extract; the `platform/resilience` transport (circuit breaker, timeout, rate limit) is already in place |
| Event lag grows although consumers are healthy | Relay throughput | Raise `AEDST_OUTBOX_BATCH_SIZE`, lower the interval, run more replicas (relays are safe to run concurrently); only then consider a broker behind the `Publisher` seam |
| Long-lived connections (realtime) or large bodies (media) | Different resource profile | Already decided: separate deployables — [ADR-0008](/decisions/0008-realtime-and-media-extraction) |

## Capacity planning arithmetic

- **Connections.** `replicas × (AEDST_DATABASE_MAX_CONNS + AEDST_DATABASE_JOBS_MAX_CONNS)`
  must stay below Postgres `max_connections` (or PgBouncer's server pool).
  Twenty replicas at the defaults is 600 connections — put PgBouncer in
  transaction mode in front of the `api` pool
  ([database.md](/database#connection-pools-and-pgbouncer)).
- **Replica sizing.** Start from the Helm chart's requests/limits and the
  HPA (CPU 70 %, memory 80 %); tune from the load test below, not from guesses.
- **Rate limits.** Three layers, all cluster-wide through Valkey so they do not
  weaken as replicas are added: per client address on the public auth RPCs
  (`AEDST_AUTH_RATE_LIMIT_PER_MINUTE`), per (account, address) plus a higher
  per-account ceiling on login (`AEDST_LOGIN_RATE_LIMIT_PER_MINUTE`; an attacker
  cannot lock the owner out from another address), and per authenticated user on
  every other RPC (`AEDST_API_RATE_LIMIT_PER_MINUTE`, default 1200). All fail
  open: a Valkey outage degrades limiting, it does not stop the API, and
  `/readyz` reports `degraded` instead of failing.
- **Load shedding.** `AEDST_MAX_IN_FLIGHT` (default 1024) caps concurrent
  requests per instance; the excess gets an immediate `429` with `Retry-After`
  instead of queueing until latency collapses for everyone. Probes are exempt.
  Size it from the load test: the p95 at which throughput stops growing is the
  limit, divided by the replica's CPU headroom.

## Load testing

`apps/server/test/load/` holds the [k6](https://k6.io) scripts:

- `api.js` — the main suite across tasks + search over Connect JSON, with three
  scenarios selected by `SCENARIO`: `smoke` (CI gate), `load` (constant arrival
  rate) and `stress` (ramping until thresholds fail). Run with
  `task load SCENARIO=smoke|load|stress RATE=… DURATION=…`; CI runs smoke, then
  load at `RATE=10`.
- `server_smoke.js` — liveness only; checks the harness, not the system.
- `scenario_tasks.js` — a realistic mix (register → create task with an
  `Idempotency-Key` → list first page and follow the cursor → complete task)
  at a constant arrival rate with per-operation p95/p99 and error-rate
  thresholds.

```bash
task up && task migrate && task run          # in one shell
task test:load:tasks RATE=50 DURATION=5m     # in another
```

Procedure to produce the numbers the triggers above need:

1. Run against a production-shaped environment (not a laptop) with a
   production-sized dataset (seed millions of tasks; keyset pagination and the
   trigram indexes only show their value at size).
2. Step `RATE` up until a threshold fails; record the highest passing rate, the
   p95/p99 per operation, and which signal in the table above moved first.
3. Record the result below, with the commit, instance sizes and dataset size.
   A number without its conditions is not a baseline.

### Recorded baselines

**2026-09-29 — single developer machine (development baseline, not a capacity figure).**
Windows 11, 16 logical cores; server, k6, PostgreSQL 17 and a Valkey stand-in
(miniredis) all on the same host, so they compete for CPU; auth and per-user API rate limits
off; one registered user shared by all virtual users; the `tasks` table held
~1.0 M rows (400 k for one hot user) plus whatever earlier runs added. Scenario:
`scenario_tasks.js`, 45–60 s hold. Each iteration = 4 requests
(create, list page 1, list page 2, complete) and produces one outbox event.

| Iterations/s (RATE) | Requests/s | p95 create / list / complete | Verdict |
|---|---|---|---|
| 50 | ~160 | 6.5 / 4.0 / 3.9 ms | all thresholds pass |
| 150 | ~460 | 11 / 5.2 / 6.1 ms | all thresholds pass |
| 250 (warm) | ~760 | 25 / 21 / 21 ms | all thresholds pass |
| 200 (first run after a restart) | ~600 | 415 / 434 / 300 ms | fails marginally — cold caches, pool still opening connections |
| 400 | ~1160 | 590 / 950 / 540 ms | fails: 2 769 dropped iterations, p95 in the hundreds of ms |

What it shows, and what it does not:

- The first signal to move under overload was the **API pool**:
  `aedst_db_pool_empty_acquire_total{pool="api"}` climbed (tens of thousands of
  waits) while the pool sat at its maximum of 20 — the pool-saturation metric
  does its job. The `jobs` pool stayed nearly idle throughout, which is the
  point of keeping them separate.
- **Event delivery was end to end correct under load**: every completed task
  produced exactly one outbox row, all were delivered, each produced exactly
  one notification (4 015 of 4 015 in the first run), and **no River job was
  created per event**. Delivery lag stayed under a second when not saturated
  (max 0.87 s observed) and drained within seconds after saturation.
- Numbers from one noisy machine with co-located dependencies say little about
  production capacity. Use them as a smoke-level regression signal and repeat
  the procedure above on a dedicated environment to fill this table properly.

**Index behaviour on the 1 M-row dataset** (`EXPLAIN ANALYZE`, hot user with
400 k tasks): fetching a page at depth 300 000 with `OFFSET` scanned 300 020
index entries and took **54.7 ms**; the same page with the keyset predicate read
21 rows and took **0.032 ms**, and the first page 0.022 ms. Ordering by due date
uses its own keyset index (0.043 ms). Substring search (`ILIKE '%…%'`) uses the
two `pg_trgm` GIN indexes (bitmap index scan, ~4.5 ms) instead of a scan of the
user's rows.

## Client compatibility and build scaling

Old app versions stay working through additive-only proto changes, a
`X-Aedst-Client` header, and a server-side minimum-version gate
(`AEDST_MIN_CLIENT_VERSIONS`) — see
[ADR-0009](/decisions/0009-api-compatibility-and-client-versioning). That ADR
also lists the per-platform build-cache recommendations (Gradle build cache,
Tuist binary cache, ccache/vcpkg binary caching) to adopt as the feature count
grows; they are recommendations and are not enabled yet.

## Known limits

Things that are deliberately not solved yet, so nobody assumes they are:

- **Only a single-machine load baseline** (above); no production-shaped measurement yet.
- **Retention is time-based only.** `platform.inbox`, `platform.idempotency_keys`
  and `auth.sessions` are purged in batches by River periodic jobs, and the
  outbox by its relay, so the tables are bounded by *age*. At very high volume
  the `DELETE`s themselves become the cost; the next step is time partitioning
  and dropping whole partitions ([ADR-0010](/decisions/0010-caching-and-data-partitioning)).
- **The shared outbox is one table.** All modules write `platform.outbox`; see
  [ADR-0012](/decisions/0012-shared-platform-messaging-tables) for when to split it.
- **No cross-relay event ordering.** Consumers must tolerate reordering
  ([events.md](/events)).
- **Idle relay polls every 500 ms per module.** Cheap (an index probe), but it
  is the latency floor; `LISTEN/NOTIFY` wake-up is a possible later
  optimization and needs the direct (non-pooled) connection.
- **The synchronous cross-module path is unproven in production code.** The
  `api/` seam and the dual-transport RPC exist and are tested in isolation, but
  no module calls another synchronously yet ([modules.md](/modules)).
- **No caching layer.** Valkey is used for rate limiting only; the strategy is
  decided in [ADR-0010](/decisions/0010-caching-and-data-partitioning) and
  implemented when a measured read path needs it.
- **Single primary.** Partitioning, replicas and sharding are a roadmap
  ([ADR-0010](/decisions/0010-caching-and-data-partitioning)), not built.
- **Infrastructure is described, not provisioned.** There is a Helm chart, but
  no Terraform/OpenTofu or Argo CD, and the chart has not been rendered against
  a cluster ([deployment.md](/deployment)).
