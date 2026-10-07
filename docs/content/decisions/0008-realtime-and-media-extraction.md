# ADR-0008: Realtime and Media Are Extracted First

## Status
Accepted; **amended for media** (see "Amendment: media ships in-process" below).

## Context
[ADR-0001](/decisions/0001-modular-monolith) keeps everything in one deployable
until a measured operational reason justifies extraction. Two upcoming
capabilities have a resource profile that is known a priori and is
fundamentally different from request/response CRUD:

- **Realtime** (chat-style message and presence fan-out, if a product adds it): tens of thousands of
  long-lived connections per node, memory-bound rather than CPU-bound, with
  deploys that must not drop every connection at once.
- **Media** (large uploads and downloads): request bodies of megabytes to
  gigabytes that would occupy API connections, saturate bandwidth and inflate
  tail latency for every other endpoint if they pass through the monolith.

Putting either inside `aedst-server` would couple the scaling and rollout of the
core API to workloads that scale on a different axis (connections, bytes). Waiting
for a production incident to prove this would be measuring something we already
know.

## Decision
`realtime` and `media` are the **first extraction candidates**, and they are
built as **separate deployables from the first line of code**: `cmd/realtime` and
`cmd/media`, each with its own Helm chart, HPA and resource profile. This is a
deliberate, pre-decided exception to the "extract when measured" rule of
ADR-0001; every other module still follows that rule (metrics such as p95 latency,
saturation or deploy-coupling incidents trigger extraction). The exception is
justified only by the known resource profile above.

They do **not** invent a new integration style. They share the same module
contract: the `api/` package seam of [ADR-0006](/decisions/0006-module-boundaries),
the Protobuf/ConnectRPC contracts ([ADR-0002](/decisions/0002-connectrpc)) and the
domain events published through the transactional outbox. They never read another
module's tables.

**Media**
- Uploads and downloads use **presigned direct-to-object-storage URLs**
  (S3-compatible: S3, R2, MinIO). The media service only authorises the request,
  issues the presigned URL (short TTL, size and content-type constraints, multipart
  for large files) and records metadata; file bytes never pass through the API
  monolith or, for uploads, through the media service either.
- Completion is confirmed by a storage event or an explicit `CompleteUpload` RPC;
  post-processing (thumbnails, virus scan) runs as River jobs.
- Downloads are served through the CDN (Cloudflare) from object storage.

**Realtime**
- The gateway is **stateless**: connection state is only the socket; the source of
  truth for messages and subscriptions is Postgres, so any gateway instance can be
  killed and clients reconnect and resume from a cursor.
- **Ephemeral fan-out uses Valkey pub-sub** (presence, typing indicators, "new
  message" nudges): it is already a platform dependency, has minimal latency and
  no persistence cost, and losing a message is acceptable because clients resync
  from the cursor. **Durable delivery uses the Postgres outbox and River** (as
  with every other event): a message is committed first and then announced.
  NATS JetStream is adopted for realtime only when cross-service fan-out with
  replay is proven necessary by measurement; until then it is not a dependency
  (see [ADR-0004](/decisions/0004-nats-jetstream)).
- Transport: **ConnectRPC server streaming** wherever the client can use it
  (Connect-ES on web, Connect-Swift, Connect-Kotlin), keeping one contract and
  one auth/interceptor stack. **WebSocket** (or SSE) is used only for clients that
  cannot consume Connect streams (for example the Qt desktop client if its stack
  lacks support), behind the same gateway.
- Graceful shutdown drains by sending a reconnect hint with jitter so a deploy does
  not cause a thundering herd.

## Amendment: media ships in-process

The media module now exists (`internal/modules/media`) and is mounted in
`aedst-server`, not a separate `cmd/media`. The reason the extraction was
pre-decided — large bodies occupying API connections — does not apply: bytes
never pass through the API (presigned PUT/GET against the `ObjectStorage` port:
S3, MinIO, R2). What *does* have a different resource profile is processing
(decode, resize, ffmpeg), and that is isolated by a dedicated River queue:

- Processing jobs run on queue `media`. `AEDST_JOBS_QUEUES` (default
  `default=10,media=2`) chooses which queues a process works; the same binary
  deployed as a worker with `media=4` and API replicas with `default=10` gives
  media its own scaling and failure domain without a second codebase.
- Extraction into its own deployable (skill `extract-service`) remains the next
  step when measurement shows the queue split is not enough.

Realtime is unchanged: still a separate deployable when a product adds it.

## Consequences
- The core API's scaling, deploys and failure domain are insulated from
  long-lived connections and large bodies from the day these features ship.
- Two extra deployables, pipelines and dashboards exist earlier than the rest of
  the system would require; the Helm chart pattern of `apps/server/deployments/helm`
  is reused to keep that cost low.
- The `api/` seam and event contracts must be respected from day one: there is no
  in-process shortcut to fall back to for these two modules.
- Valkey pub-sub is at-most-once. Any feature needing guaranteed delivery must use
  the outbox path, and this must be visible in the design of each realtime feature.
- Presigned URLs move authorisation to URL issuance: TTLs, scoped keys and
  content-length constraints are security-critical and need tests.
- If the measured triggers for other modules (see ADR-0001 and
  [architecture.md](/architecture#extraction-path)) fire later, they follow the same
  pattern.
