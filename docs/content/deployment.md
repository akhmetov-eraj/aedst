# Deployment

## Local development (implemented this phase)

`deployments/docker/docker-compose.yml` runs PostgreSQL 18, Redis 8, and a
single-node NATS JetStream — everything `aedst-server` needs. `make dev` starts
the compose stack, waits for it, runs migrations, and runs the server with
live-reload disabled (use `go run` directly for iterative work). See the
[server README](../../apps/server/README.md).

## Target production architecture (documented, not yet provisioned)

```
                 Cloudflare
                     |
                   Envoy
                     |
              ConnectRPC / API
                     |
       AEDST Modular Monolith (Kubernetes Deployment)
                     |
       +-------------+-------------+
       |             |             |
  PostgreSQL       Redis          NATS
  (+ PgBouncer)   (managed)      JetStream
```

This phase does **not** ship Helm charts, Terraform/OpenTofu modules, or a
GitHub Actions deploy pipeline — only local dev tooling and a CI job that lints,
generates, tests and builds. When a real cluster target exists:

- **Kubernetes**: one `Deployment` for `aedst-server` (stateless, horizontally
  scaled — see the [scalability principles](../../CLAUDE.md) baked into
  `platform/*`: no in-process session state, idempotent handlers). Config via
  `ConfigMap`/`Secret`, consumed through `platform/config` (`caarlos0/env`) so
  no code changes are needed between environments.
- **Helm**: a single chart with per-environment `values-{dev,staging,prod}.yaml`
  rather than duplicated manifests.
- **OpenTofu/Terraform**: separate modules for network, database (managed
  Postgres), redis, nats, kubernetes cluster, observability stack, edge
  (Cloudflare/Envoy), and secrets — provisioned independently so a module's
  infra can be recreated without touching the others.
- **Edge**: Cloudflare in front of Envoy; the app is never exposed directly to
  the public internet.
- **CD**: Argo CD watching the Helm chart's rendered manifests per environment.

Module extraction (Phase 2+) changes this to one `Deployment`/chart per
extracted service behind the same Envoy boundary — see
[architecture.md#extraction-path](/architecture#extraction-path).
