# Event Architecture

Integration events are how a module observes another module's state changes
without a synchronous call — see [modules.md](/modules) for which modules are
event-only consumers, and [ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus)
for why the transport is what it is.

```
domain change + outbox row            (one Postgres transaction; platform.outbox, module column)
        │
        ▼
  Relay (per module, batched)         claims rows with FOR UPDATE SKIP LOCKED
        │
        ▼
  messaging.Bus (in-process)          fans out by event type
        │
        ▼
  inbox.Consumer (per subscriber)     dedupes on event_id, applies the effect and
                                      the "processed" marker in one transaction
```

There is one asynchronous mechanism family: **Postgres outbox → relay → bus →
inbox**. River is separate and is for a module's own deferred work (see
[ADR-0005](/decisions/0005-river-for-jobs)); it is not in the event path.

## Conventions

- **Type**: `<module>.v<version>.<EventName>`, e.g. `tasks.v1.TaskCompleted`.
- **Contract**: the event type constant and payload struct live in the
  producer's `api/` package (`modules/<module>/api/events.go`) — the only
  package a consumer may import. Payloads are JSON while every consumer is
  in-process; when an event has to cross a service boundary, promote it to a
  protobuf message under `api/proto/aedst/<module>/v1/events.proto` with the
  same field names.
- **Evolution**: within a version fields are only added — consumers ignore
  unknown fields, and nothing is renamed, retyped or removed. A breaking
  change publishes a new version (`…v2`) next to the old one until every
  consumer has moved, then the old one is retired.
- **Self-contained payloads**: an event carries what a consumer needs to build
  its projection (e.g. `TaskCompleted` includes the task title) so consumers
  never call back into the producer. If a consumer would need to, the payload
  is wrong.
- **Identity**: every event has an `event_id` (UUIDv7, the outbox row id),
  stable across redeliveries. Consumers dedupe on it.
- **Delivery**: at-least-once. The relay retries with exponential backoff
  (1s, 2s, 4s … capped at 5 minutes) and poisons a row after 10 failed
  attempts; poisoned rows stay in the table for inspection and replay.
  Consumers must be idempotent — `platform/inbox` gives effectively-once
  *effects* by recording the `event_id` in the same transaction as the effect.
- **Ordering**: events of one aggregate are written in commit order and claimed
  oldest-first, but with several relays (replicas) or retries there is **no
  ordering guarantee**. Consumers that care compare a version/timestamp carried
  in the payload; `partition_key` (the aggregate id) is recorded on every event
  so a broker with per-key ordering can be introduced later without touching
  producers.
- **Tracing**: the originating W3C `traceparent` is stored on the row and
  restored on delivery, so a consumer's work appears in the trace of the
  request that caused it.

## Operating the relay

Configuration (see `AEDST_OUTBOX_*` in `.env.example`):

| Variable | Default | Meaning |
|---|---|---|
| `AEDST_OUTBOX_RELAY_INTERVAL_MS` | 500 | Idle poll interval — the latency floor of every event. |
| `AEDST_OUTBOX_BATCH_SIZE` | 100 | Rows claimed per pass; a full pass runs again immediately. |
| `AEDST_OUTBOX_CONCURRENCY` | 4 | Events of a batch published in parallel (each subscriber runs its own transaction on the jobs pool). |
| `AEDST_OUTBOX_RETENTION_HOURS` | 168 | Delivered rows older than this are purged. |

Metrics (OTLP; Prometheus `/metrics` only if enabled, off by default), all labelled `module`:

| Metric | Alert on |
|---|---|
| `aedst_outbox_oldest_pending_age_seconds` | > 30 s for 5 min — delivery is lagging or a consumer is stuck. |
| `aedst_outbox_pending` | Rising without bound (capped at 10 000). |
| `aedst_outbox_failed_total` | Sustained non-zero rate — a subscriber is failing and being retried. |
| `aedst_outbox_poisoned_total` | **Any increase** — an event was given up on; inspect `poisoned_at IS NOT NULL` rows. |
| `aedst_outbox_delivered_total`, `aedst_outbox_batch_size` | Throughput and batching efficiency. |

Any number of server replicas may run the relay concurrently: rows are leased
(`FOR UPDATE SKIP LOCKED` plus a `next_retry_at` lease), so no row is delivered
by two relays at once, and a relay that dies mid-batch simply lets its lease
expire (60 s) so the rows are claimed again.

Replaying a poisoned event: clear `poisoned_at` and `attempts` on its row; the
next relay pass picks it up.

## Catalog

| Event | Version | Producer | Payload (`api.TaskCompletedEvent`) | Consumers |
|---|---|---|---|---|
| `tasks.v1.TaskCompleted` | 1 | `tasks` (on `CompleteTask`, atomically with the status change) | `task_id`, `user_id`, `title`, `completed_at` | `notifications` (projects a "Task completed" feed entry) |

A new event adds a row here, its type and payload to the producer's
`api/events.go`, and its consumers in the same PR that introduces it.

## Known limits

- Delivery runs in the same process as the producer. That is the right trade
  while every consumer is in-process; the day a consumer becomes its own
  service, the relay's `messaging.Publisher` is swapped for a broker adapter
  (the seam is one interface) and `partition_key` becomes the broker's
  ordering key. See [ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus).
- Retention: delivered outbox rows are purged by their relay
  (`AEDST_OUTBOX_RETENTION_HOURS`); `platform.inbox` and
  `platform.idempotency_keys` are purged by River periodic jobs
  (`internal/platform/retention`, `AEDST_RETENTION_*`). The inbox retention must
  not be shorter than the outbox retention — config validation enforces it —
  or a late redelivery could be applied twice once its marker is gone.

## Producing an event exactly once

A state transition that publishes an event must be *guarded in SQL*, not only
checked in Go: `UPDATE … WHERE status <> 'done' RETURNING …` returns no row when
the task is already done (a retry, a double click, two devices), and the
repository then writes **no** outbox row. Otherwise the second call would mint a
second `event_id`, slip past every consumer's inbox and produce a duplicate
effect. `tasks.Repository.Complete` is the reference; its integration test
completes one task from eight goroutines and asserts exactly one outbox row.
