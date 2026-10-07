# ADR-0005: River for background jobs

## Status
Accepted

## Context
Background work (email, push notifications, media processing, search
indexing, cleanup, scheduled tasks) needs reliable, transactional job
scheduling that doesn't require operating a second piece of broker
infrastructure beyond what's already required.

## Decision
Use River, which stores jobs in the same PostgreSQL cluster already used as the
system of record. Jobs can be enqueued in the same transaction as the state
change that triggers them, giving exactly the "committed implies scheduled"
guarantee events would otherwise need an outbox for.

## Consequences
- No separate job-queue infrastructure (e.g. Redis-backed queues) to operate.
- Jobs and events stay conceptually distinct: the outbox + relay + in-process
  bus ([ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus)) is for
  cross-module *notification* of state changes (fan-out, unknown consumers),
  River is for a module's own *deferred work* (single owner, needs a retry
  policy). A module should not use River as a pub/sub bus, and the event path
  no longer enqueues a River job per event.
- Job throughput is bounded by the same Postgres cluster serving transactional
  reads/writes; acceptable at current scale, revisit only if a specific job
  type's volume threatens the primary workload — at which point that job type
  moves to its own River queue/database, not a different job system.
