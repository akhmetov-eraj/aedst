# Modules

AEDST is an empty product template. It ships only the **global modules** almost
every product needs, with `tasks` as the worked example every other module is
built around. Your product adds its own domain modules next to them, following
the same shape.

| Module | Owns (schema) | Depends on (via `api/`) | Contract |
|---|---|---|---|
| `auth` | `auth.*` (credentials, sessions, identity; user directory for admin) | — | `aedst.auth.v1` |
| `tasks` | `tasks.*` | — (scopes every RPC to the caller's `user_id` from the auth context) | `aedst.tasks.v1` |
| `notifications` | `notifications.*` (inbox, push devices, preferences) | events only: `tasks` (`TaskCompleted`, `TaskDueSoon`), `media` (`MediaReady`) | `aedst.notifications.v1` |
| `search` | `search.*` (read-only `documents` projection) | events only: `tasks` (`TaskCreated/Updated/Completed/Deleted`), `media` (`MediaReady/Deleted`) | `aedst.search.v1` |
| `media` | `media.*` (+ objects in S3 / MinIO / R2) | `tasks` (`api/`: ownership check for attachments) | `aedst.media.v1` |
| `admin` | `admin.*` (audit log only) | `auth`, `tasks`, `media`, `notifications` (read-mostly composition) | `aedst.admin.v1` |

Contracts live in `apps/server/api/proto/aedst/<module>/v1/` and are the single
source of truth for all five clients (Web, Android, Apple, Desktop).

## Dependency graph

```
auth          tasks ◄──────────── media (api/: "does this task belong to the caller?")
                │                    │
                │ events             │ events
                ▼                    ▼
        ┌── notifications ◄──────────┤   inbox + push (Web Push / FCM / APNs)
        └── search        ◄──────────┘   projection (Postgres FTS + pg_trgm)

admin ──→ api/ of auth, tasks, media, notifications  (nothing depends on admin)
```

Rules this graph encodes:

- No cycles. `auth` and `tasks` import no other module.
- `notifications` and `search` are **consumers only**: they react to integration
  events published through the outbox, inside an inbox transaction, and never
  receive synchronous calls from the modules that feed them. That is what lets
  them become independent services, or be rebuilt from scratch by replaying
  events, without touching the producers.
- `admin` may depend on many modules' `api/` packages because it is a
  composition layer; nothing depends on `admin`. Its audit entry is written in
  the same transaction as the effect, through the target module's `…Tx` api
  method — the one documented exception to "a transaction never spans modules"
  (`apps/server/AGENTS.md`).

Every new module adds a row + graph edges here in the PR that introduces it, and
is checked by `architecturetest` before merge.

## What each global module does

### Search

- Projection `search.documents` built from events; newest `version` wins, deletes
  leave tombstones that retention purges.
- `Search`: `websearch_to_tsquery` + `ts_rank_cd`, snippets from `ts_headline`
  with U+E000 / U+E001 around matches so every client renders highlighting
  natively. `Suggest`: `pg_trgm` prefix + word similarity.
- Always scoped to the caller. Keyset pagination on `(score, id)`.
- Client UIs: Web command palette (`cmdk`) + `/search`; Android/Apple/Desktop
  search screens with a 300 ms debounce.

### Notifications

- In-app inbox (list, unread count, mark read / all read) and per-kind
  preferences.
- Push fan-out as River jobs: one `notifications.push` job per notification, one
  `notifications.push_device` job per device. Providers: **Web Push** (VAPID),
  **FCM** (HTTP v1), **APNs** (HTTP/2, token auth). An unconfigured provider is
  disabled; an invalid token deactivates the device.
- Payload contract — FCM `data` / APNs custom keys / Web Push JSON all carry
  `kind`, `title`, `body`, `link` (relative in-app route) and
  `notification_id`.
- Desktop has no push service: it polls `GetUnreadCount` with backoff and shows
  tray notifications.

### Media

- Bytes never pass through the API: `CreateUpload` returns a presigned PUT,
  the client uploads directly to object storage, then calls `CompleteUpload`.
- Completion verifies size (`Head`) and sniffed content type, then enqueues a
  job on the River queue `media` that produces variants (thumbnail, preview,
  optimized; poster frame for video when ffmpeg is configured) and publishes
  `MediaReady`.
- Storage is the `ObjectStorage` port with presets for AWS S3, MinIO and
  Cloudflare R2. Abandoned uploads are cleaned up by retention.
- Clients: `media/{image, video, player, uploader, thumbnail, compression}` on
  every platform; images are compressed on the device before upload.

### Admin

- Overview (users, tasks, media, push devices, outbox lag, pending jobs),
  users, all tasks (delete with a reason), broadcast notification, audit log.
- Closed by default: `/aedst.admin.v1.AdminService/*=admin` is a built-in role
  policy enforced by `RoleInterceptor`; clients show the admin area only when
  the token carries the `admin` role, but the server is the authority.

## Module internal structure

```
internal/modules/<name>/
    domain/            entities, value objects, domain errors — stdlib and
                        internal/shared only
    application/       commands, queries, ports.go (interfaces they need)
    infrastructure/     postgres/, events/ (consumers), jobs — implement ports
    transport/           ConnectRPC handlers — depend on application only
    api/                the module's public boundary: the only package other
                        modules (or admin) may import; events.go holds the
                        integration events it publishes
```

`api/` re-exports just enough of `application` for other modules to call
in-process today and over ConnectRPC tomorrow — it is the seam the
[extraction path](/architecture#extraction-path) cuts along.

## Reference implementations

- **`tasks`** — request/response module: keyset pagination, idempotent
  commands, optimistic concurrency, outbox-published integration events.
- **`notifications`** — event consumer: subscribes through the producer's
  `api/` only, applies the effect inside an inbox transaction (exactly-once
  effect), stores its own projection, delivers side effects (push) from River.
- **`media`** — external resource behind a platform port (`ObjectStorage`) with
  asynchronous processing on its own job queue.
- **`admin`** — composition over other modules' `api/` with an audit trail.

## Adding your own module

1. Write the spec (`apps/server/specs/<module>/`), then the proto in
   `api/proto/aedst/<module>/v1/`.
2. Scaffold `internal/modules/<module>/` with the layout above, a goose
   migration directory and an own schema; add the module to `MODULES` in
   `apps/server/Taskfile.yml`.
3. Mount the service through `mountService` in `cmd/server` (protovalidate,
   body limits and the Connect header check come with it) and add its
   descriptor to `knownProcedures`.
4. Publish events through the outbox; let `search`/`notifications` consume them
   if your entities should be searchable or notify users.
5. Add a row and edges to this page.
