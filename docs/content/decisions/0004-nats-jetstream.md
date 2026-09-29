# ADR-0004: NATS JetStream for domain/integration events

## Status
Accepted

## Context
Several planned modules (`notifications`, `search`, `recommendations`,
`moderation`) must react to state changes in other modules without a
synchronous dependency, and without those producer modules knowing who
consumes their events — required both for module boundary rules (see
[ADR-0006](/decisions/0006-module-boundaries)) and for future independent scaling.

## Decision
Use NATS JetStream for domain/integration events (see
[events.md](/events) for subject naming, versioning and idempotency
conventions). Kafka is explicitly not introduced — JetStream's operational
footprint is far lower and nothing in the current requirements (retention,
throughput, stream processing) needs Kafka's specific capabilities.

## Consequences
- One broker to operate for both pub/sub events and (via `platform/jobs`,
  backed by River on Postgres, not NATS) background jobs — the two stay
  conceptually and technically separate (`events = NATS`, `jobs = River`).
- At-least-once delivery means every consumer must be idempotent; this is a
  hard requirement, not a suggestion (see `events.md`).
- Revisit if a concrete requirement appears for multi-day+ replay at high
  volume, ordered multi-partition consumption, or stream processing — none of
  which exist today.
