# Deployment

## Local development

`apps/server/deployments/docker/docker-compose.yml` runs PostgreSQL 18 and Valkey
(the only services `aedst-server` needs). Optional compose profiles add
observability (`observability`), a Keycloak OIDC provider (`idp`) and PgBouncer
(`pgbouncer`, transaction pooling on `localhost:6432`) for rehearsing the production
connection path. The `observability` profile is a single `grafana/otel-lgtm` container (OTLP in). NATS is
not used: asynchronous work runs on an in-process bus
(`internal/platform/messaging`) fed by the Postgres transactional outbox and River
jobs (see [events](/events) and [ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus)).
`task dev` (in `apps/server`) starts the stack, applies migrations and runs the
server. See the [server README](../../apps/server/README.md).

## Scalability principles

These are baked into `internal/platform/*` and every module must preserve them:

- **Stateless processes**: no in-process session or request state; any replica can
  serve any request, so horizontal scaling is just more pods.
- **Idempotent handlers**: commands accept an `Idempotency-Key` and dedupe in the
  same transaction as the write (`platform/idempotency`); consumers dedupe through
  the inbox (`platform/inbox`).
- **Transactional outbox**: state changes and the events they cause commit together;
  a relay publishes afterwards (`platform/outbox`), so no dual writes.
- **Bounded pools**: the API DB pool (`AEDST_DATABASE_MAX_CONNS`) and the background
  jobs pool (`AEDST_DATABASE_JOBS_MAX_CONNS`) are limited per pod so replicas x pool
  size stays under Postgres/PgBouncer capacity and background work cannot starve
  the API.
- **Graceful shutdown**: on SIGTERM `/readyz` returns 503 first
  (`AEDST_SHUTDOWN_DRAIN_SECONDS`), then the listener stops and in-flight requests
  finish (`AEDST_SHUTDOWN_TIMEOUT_SECONDS`).
- **Configuration via environment only** (`platform/config`, `caarlos0/env`): the
  same image runs in every environment and startup fails fast on invalid config.

Related decisions: [ADR-0008 realtime and media extraction](/decisions/0008-realtime-and-media-extraction),
[ADR-0009 API compatibility and client versioning](/decisions/0009-api-compatibility-and-client-versioning),
[ADR-0010 caching and data partitioning](/decisions/0010-caching-and-data-partitioning).

## Target production architecture

```
                 Cloudflare
                     |
               Envoy / Ingress
                     |
              ConnectRPC / API
                     |
       AEDST Modular Monolith (Kubernetes Deployment)
                     |
       +-------------+-------------+
       |                           |
  PostgreSQL                    Valkey
  (via PgBouncer,               (managed)
   + direct connection
   for jobs/locks)
```

### Status

| Item | Status |
| --- | --- |
| Dockerfile (`deployments/docker/Dockerfile`, distroless nonroot) | Implemented |
| Migration image (`deployments/docker/Dockerfile.migrate`, goose) | Implemented |
| Helm chart `apps/server/deployments/helm/aedst-server` | Implemented (not yet linted in CI) |
| PgBouncer compose profile for local rehearsal | Implemented |
| OpenTofu/Terraform modules | Not implemented |
| Argo CD | Not implemented |
| GitHub Actions deploy pipeline | Not implemented |
| Realtime / media deployables (ADR-0008) | Not implemented |

### Helm chart

One chart with `values-{dev,staging,prod}.yaml` overlays:

```bash
helm upgrade --install aedst-server apps/server/deployments/helm/aedst-server \
  -n aedst -f apps/server/deployments/helm/aedst-server/values-prod.yaml \
  --set image.tag=<git-sha>
```

- `Deployment` (rolling update, zero unavailable, topology spread over zones and
  nodes, read-only root filesystem, non-root), `Service`, `ConfigMap`, `HPA`
  (`autoscaling/v2`, CPU and memory), `PodDisruptionBudget`, `ServiceAccount`, and
  an optional `ServiceMonitor`.
- Liveness probes `/livez`, readiness probes `/readyz`. No `preStop` hook is used:
  the server fails readiness first on SIGTERM.
  `terminationGracePeriodSeconds` is computed as drain + shutdown timeout + 10s and
  the render fails if an override is not larger than drain + timeout.
- **Secrets are never in values.** `existingSecret` names a Secret provisioned
  elsewhere (External Secrets, sealed secrets or SOPS); the chart references its keys
  for the DSNs, the JWT signing key and the Valkey password.
- A **migration Job** runs as a `pre-install,pre-upgrade` Helm hook and applies goose
  migrations for every module before new pods roll out. It uses the direct DSN when
  configured.

### PgBouncer and the database

- In production `AEDST_DATABASE_URL` should point at **PgBouncer in transaction
  pooling mode**. pgx's default named prepared-statement cache does not work in that
  mode, so add `default_query_exec_mode=cache_describe` (or `exec` /
  `simple_protocol`) to the DSN. PgBouncer must also ignore the startup parameters
  the server sends (`ignore_startup_parameters=extra_float_digits,statement_timeout,idle_in_transaction_session_timeout`);
  set the timeouts on the database role instead.
- **Session-level advisory locks** (`internal/platform/lock`) and **River's
  LISTEN/NOTIFY** need a direct, non-pooled connection. Provide it as
  `AEDST_DATABASE_DIRECT_DSN` (Helm: `database.directDsnSecretKey`); when set, the
  jobs pool (River, outbox, inbox) uses it instead of the main DSN.
- Size pools so `replicas x (MAX_CONNS + JOBS_MAX_CONNS)` fits the direct-connection
  budget of Postgres, and PgBouncer's `default_pool_size` for the API side.

### High-availability PostgreSQL

`apps/server/deployments/k8s/postgres/cluster.yaml` is a [CloudNativePG](https://cloudnative-pg.io)
`Cluster` — cloud-agnostic, runs on any conformant cluster: 3 instances on
different nodes (required anti-affinity, zone spread), **one synchronous replica**
(an acknowledged commit survives losing the primary), automatic failover,
continuous WAL archiving plus a nightly base backup to S3-compatible storage
(point-in-time recovery, 30-day retention), and a 3-replica **PgBouncer `Pooler`**
in transaction mode for the API pool. The jobs pool connects to the cluster
service directly (`AEDST_DATABASE_DIRECT_DSN`). Managed Postgres (RDS, Cloud SQL,
…) is an equally valid choice; the requirements are the same: synchronous
replica, PITR, and a direct (non-pooled) endpoint for the jobs pool.

What this buys, and what it does not: a node or zone loss fails over in seconds
with no committed data lost; a bad migration or `DELETE` is recoverable to the
second. Recovery is only real once **restored**: rehearse a restore into a scratch
namespace (`bootstrap.recovery` from the object store) before relying on it, and
after every major upgrade.

### GitOps (Argo CD)

`apps/server/deployments/k8s/argocd/applications.yaml` declares an `AppProject`
and one Application per environment: **staging** follows `main` and syncs
automatically (prune and self-heal; the database is never pruned); **prod**
follows a release tag and syncs **manually**, so a person promotes a build after
staging is green. The chart's migration Job runs as a PreSync hook, so
migrations complete before any new pod starts.

### Alerts, dashboard and runbook

The chart ships `PrometheusRule` alerts (`files/alerts.rules.yaml`, native
Prometheus format) and a Grafana dashboard (`files/aedst-server-dashboard.json`),
enabled per environment with `alerts.enabled` / `dashboards.enabled`. Every alert
links to its section of the [runbook](/runbook). The alerts cover no-instances,
5xx ratio, p95 latency, throttling/shedding, connection-pool saturation and
waiting, outbox lag / growing backlog / failing delivery / **poisoned events**
(paged), and goroutine growth — all from the server's own OpenTelemetry metrics.
Optional `networkPolicy` restricts ingress to the ingress controller and egress
to DNS plus what you list (`egressTo` must name Postgres, Valkey, the OTLP
collector and the OIDC provider, which is why it is opt-in).

### Verification status (read this before trusting the manifests)

| Artifact | Verified how |
|---|---|
| Helm chart incl. `PrometheusRule`, dashboard ConfigMap, `NetworkPolicy` | Rendered with the Helm engine (defaults, prod values, and a values file enabling every optional template) |
| Alert expressions | Each PromQL expression parses with Prometheus' own parser; metric names checked against a running server's `/metrics`. **Not** evaluated against real traffic |
| Dashboard | Generated JSON, structure checked; not imported into Grafana |
| CloudNativePG cluster, Argo CD applications | YAML syntax and `apiVersion`/`kind` checked only. **Never applied to a cluster** — run `kubectl apply --dry-run=server` against your operator/Argo versions first; fields marked `CHECK` depend on the operator version |

### Still to do when a real cluster target exists

- **OpenTofu/Terraform**: deliberately not written yet. The modules (network,
  managed or self-hosted Postgres, cluster, edge) are a function of the chosen
  cloud, and guessing a provider produces code nobody can apply. Pick the cloud,
  then add one module per concern so each can be recreated independently.
- **Edge**: Cloudflare in front of Envoy/ingress; the app is never exposed directly
  to the internet. Set `AEDST_TRUSTED_PROXY_HOPS` to the real proxy count.
- **A production-shaped load test** ([scaling.md](/scaling#load-testing)): the only
  recorded numbers are from one developer machine. Alert thresholds and
  `AEDST_MAX_IN_FLIGHT` should be re-derived from it.
- Extraction of realtime and media as their own charts, see ADR-0008.
