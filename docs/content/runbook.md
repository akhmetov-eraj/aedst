# Runbook

One section per alert in `apps/server/deployments/helm/aedst-server/files/alerts.rules.yaml`; each alert's `runbook_url` points at the heading with the same name. Dashboard: the `aedst-server` Grafana dashboard (same chart). Logs: every server-side failure is logged as `rpc failed` with `procedure`, `code`, the real `error`, and `trace_id`/`request_id`; find the trace from the log line, not the other way round.

Severity `page` = act now, `ticket` = fix in working hours.

## AedstNoInstancesReporting

**Meaning.** No pod exports metrics: every replica is down, crash-looping, or the scrape is broken.
1. `kubectl -n <ns> get pods -l app.kubernetes.io/name=aedst-server` — any `Running/Ready`? If yes, the problem is scraping (ServiceMonitor selector, NetworkPolicy blocking the metrics port), not the service.
2. Crash loop: `kubectl logs --previous <pod>`. Startup fails fast and loudly on bad config (`config: …`), an unreachable database (`database: ping`), or an invalid role policy (`role policy names unknown procedure …`). Fix the value and roll out.
3. Failed migration Job blocks the rollout: `kubectl get job -l app.kubernetes.io/component=migrate`, read its logs. A `lock_timeout` error means DDL queued behind traffic — retry off-peak.

## AedstHighErrorRatio

**Meaning.** More than 2% of requests are 5xx for 10 minutes.
1. Dashboard → *Requests per second by status* and *p95 latency by RPC method*: one method or all?
2. Logs: `rpc failed` lines carry the cause. Group by `error`; typical causes are the database (`context deadline exceeded`, `too many connections`), a bad deploy (compare with the rollout time), or a dependency of a module.
3. If the cause is a bad deploy: `helm rollback aedst-server`. Migrations are forward-compatible by policy (squawk), so rolling the binary back is safe.
4. Database pool exhaustion → see [AedstDBPoolWaiting](#aedstdbpoolwaiting).

## AedstHighLatencyP95

**Meaning.** p95 above 500 ms for 10 minutes (429 and 5xx excluded — they have their own alerts).
1. *p95 latency by RPC method* — is it one RPC? A list endpoint with a search term is the usual suspect (`ILIKE` on a table larger than the trigram index helps with).
2. *Connections in use vs max* and *Time spent waiting for a connection*: if the pool is saturated, latency is queueing, not slow queries.
3. `pg_stat_statements` ordered by `total_exec_time`; `EXPLAIN (ANALYZE, BUFFERS)` the top query. Keyset queries must use their `(user_id, sort_key, id)` index — a sequential scan means a missing or unusable index.
4. CPU-bound pods (HPA at max) → raise `autoscaling.maxReplicas`, then check `database.maxConns` × replicas against Postgres `max_connections` ([scaling.md](/scaling)).

## AedstShedding429

**Meaning.** More than 1 request/s is being answered 429.
- Per-user limit (`AEDST_API_RATE_LIMIT_PER_MINUTE`): one user or client is hot. Find it: `rl:user:<id>` keys, or the `user_id` on 429 log lines. A misbehaving client is fixed in the client; a legitimately busy integration may need a higher limit.
- Load shedding (`AEDST_MAX_IN_FLIGHT`): the instance is saturated. Scale out first; raise the cap only if CPU is idle while requests are shed.

## AedstDBPoolSaturated

**Meaning.** A pool is above 80% in use for 5 minutes (`api` = requests, `jobs` = relay, River, event consumers).
1. Long transactions: `SELECT pid, now() - xact_start AS age, state, left(query, 80) FROM pg_stat_activity WHERE application_name LIKE 'aedst-server%' ORDER BY age DESC LIMIT 10;`
2. `api` pool: slow queries (see AedstHighLatencyP95) or too few connections for the traffic. `jobs` pool: a burst of events or a slow subscriber.
3. Adding connections is the last resort: `replicas × (MAX_CONNS + JOBS_MAX_CONNS)` must stay under Postgres `max_connections` / PgBouncer's server pool.

## AedstDBPoolWaiting

**Meaning.** Callers spend more than 0.1 s per second waiting for a connection: the pool is the bottleneck now.
Follow [AedstDBPoolSaturated](#aedstdbpoolsaturated) step 1 immediately. If a single slow query or an idle-in-transaction session holds connections, terminate it (`SELECT pg_terminate_backend(pid)`), then fix the cause. `idle_in_transaction_session_timeout` (30 s) and `statement_timeout` (15 s) already bound the damage.

## AedstOutboxLagging

**Meaning.** The oldest undelivered event of a module is older than 30 s.
1. Is the relay running? Logs of the pods for `outbox:` lines; one relay per module per pod starts with the process.
2. Failing subscribers: `aedst_outbox_failed_total` rising → see [AedstOutboxDeliveryFailing](#aedstoutboxdeliveryfailing).
3. Nothing failing but lag grows: the jobs pool is saturated ([AedstDBPoolSaturated](#aedstdbpoolsaturated)) or the relay cannot keep up — raise `AEDST_OUTBOX_BATCH_SIZE` and `AEDST_OUTBOX_CONCURRENCY`, or add replicas (relays lease rows, so they cooperate).
4. Inspect: `SELECT module, count(*), min(occurred_at) FROM platform.outbox WHERE published_at IS NULL AND poisoned_at IS NULL GROUP BY module;`

## AedstOutboxBacklogGrowing

**Meaning.** More than 1000 events pending and the number rising for 15 minutes: producers outpace delivery. Same steps as [AedstOutboxLagging](#aedstoutboxlagging); if delivery is healthy, the sustained event rate needs more relay throughput (`AEDST_OUTBOX_*`) — the point at which [ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus) says to consider a broker behind `messaging.Publisher`.

## AedstOutboxDeliveryFailing

**Meaning.** A subscriber keeps failing; events are retried with exponential backoff (1 s … 5 min, 10 attempts).
1. Logs: `outbox: delivery failed, will retry` with `event_type`, `event_id`, `attempt` and the subscriber's error.
2. Fix the subscriber (or its dependency). Retries succeed on their own once it is healthy — no manual action, and the inbox guarantees the effect is applied once.
3. Fix it before attempt 10, or the event is poisoned ([AedstOutboxEventPoisoned](#aedstoutboxeventpoisoned)).

## AedstOutboxEventPoisoned

**Meaning.** An event exhausted its attempts and is parked; its consumers are missing it. Paged on purpose: this is silent data divergence.
1. Find it: `SELECT event_id, module, event_type, occurred_at, attempts FROM platform.outbox WHERE poisoned_at IS NOT NULL ORDER BY poisoned_at DESC;` and the `event poisoned after exhausting attempts` log line with the error.
2. Fix the root cause (a subscriber bug, a bad payload).
3. Replay once the fix is deployed: `UPDATE platform.outbox SET poisoned_at = NULL, attempts = 0, next_retry_at = NULL WHERE event_id = '<id>';` — the relay picks it up on its next pass, and the inbox dedupes any consumer that already applied it.
4. An event that can never be delivered (its payload is invalid for good) stays poisoned as a record; note it in the incident. Poisoned rows are kept on purpose and are not purged with published ones.

## AedstGoroutinesHigh

**Meaning.** A pod holds more than 10000 goroutines for 15 minutes.
Usually requests stuck on a slow dependency (check the pool alerts) or a leak. Capture before restarting: `kubectl port-forward <pod> 9090` is the metrics port only — take a goroutine dump by sending `SIGQUIT` to the pod's process (`kubectl exec` is unavailable on the distroless image; use `kubectl debug --profile=general` with a debug container, or `kill -QUIT 1` from an ephemeral container sharing the process namespace) and read the stack that repeats. Then restart the pod.

## Related non-alerting checks

- **Retention** (`aedst_retention_purged_total`, `retention: purged rows` logs): inbox, idempotency keys and dead sessions are purged hourly by River periodic jobs. If table sizes keep growing, look for failed `platform.retention` jobs: `SELECT state, count(*) FROM river_job WHERE kind = 'platform.retention' GROUP BY 1;`
- **Valkey down**: `/readyz` reports `degraded`, limiters fail open, one warning per 30 s is logged. Restore Valkey; nothing needs to be redeployed.
