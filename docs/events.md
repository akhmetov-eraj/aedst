# Event Architecture

Transport: **NATS JetStream** (`platform/messaging`). Events are the only way a
module observes another module's state changes without a synchronous call — see
[modules.md](modules.md) for which modules are event-only consumers.

## Conventions

- **Subject**: `aedst.<module>.<event>.v<version>`, e.g.
  `aedst.profile.profile_updated.v1`.
- **Payload**: a protobuf message defined alongside the module's API proto
  (`api/proto/<module>/v1/events.proto`), so publishers and consumers share a
  generated type and versioning follows the same buf breaking-change rules as
  RPCs.
- **Idempotency**: every event carries an `event_id` (UUIDv7); consumers persist
  processed IDs (or use a natural idempotency key from the payload) before
  acting, so redelivery from JetStream never double-applies an effect.
- **Delivery**: at-least-once. Consumers must be idempotent; JetStream durable
  consumers with explicit ack are used, not core NATS fire-and-forget.
- **Outbox**: modules that must guarantee "committed to Postgres implies
  eventually published" write the event to an `outbox` table in the same
  transaction as the state change, and a relay (polling or `pg_notify`-driven)
  publishes it to JetStream. Modules without that reliability requirement may
  publish directly after commit. Start with direct publish; add the outbox
  table to a module only once a concrete reliability gap is observed there.
- **Observability**: every published/consumed event carries the originating
  `trace_id` in a NATS header so a consumer's processing shows up in the same
  distributed trace as the request that caused it.

## Catalog (Phase 1)

| Event | Publisher | Consumers |
|---|---|---|
| `UserRegistered` v1 | `users` | `notifications` (future), `profile` (creates default profile) |
| `ProfileUpdated` v1 | `profile` | `notifications` (future), future read-model consumers (`social`, `marketplace`, `search`, `recommendations`) |

Future modules add rows here (and a `.proto` under `api/proto/<module>/v1/`) in
the same PR that introduces the event.
