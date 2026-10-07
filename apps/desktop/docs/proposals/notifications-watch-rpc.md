# Proposal for the backend team: `NotificationsService.WatchNotifications` (server streaming)

- From: `apps/desktop` (Phase 6 of the desktop plan)
- To: owners of `apps/server` / `api/proto/aedst/notifications/v1`
- Status: **proposal only** - nobody has changed a `.proto`. The desktop client is ready for it (see "Client readiness") and keeps polling until this RPC exists.

## Why

Desktop has no FCM/APNs/Web Push channel. Today it polls `GetUnreadCount` every 30 s (foreground) / 2 min (background)
and fetches the newest unread items when the count rises: up to 30 s of latency per notification, constant idle traffic
per signed-in client, and no way to be told about a notification while a request is in flight. A server-streaming RPC
removes the poll and delivers notifications within a second. Web could use the same RPC through Connect-ES streaming.

## Proposed contract

```proto
service NotificationsService {
  // ...existing RPCs...

  // Server stream of events for the caller's inbox. The first event is always an unread-count snapshot.
  // The client reconnects with backoff and resumes with last_event_id.
  rpc WatchNotifications(WatchNotificationsRequest) returns (stream NotificationEvent);
}

message WatchNotificationsRequest {
  // Opaque id of the last event the client processed; empty = "from now" (the snapshot is still sent).
  string last_event_id = 1 [(buf.validate.field).string.max_len = 128];
}

message NotificationEvent {
  // Monotonic and totally ordered per user, opaque to the client. Present on every event except heartbeats.
  string event_id = 1 [(buf.validate.field).string.max_len = 128];
  oneof event {
    // A notification was created for the caller (same message as ListNotifications returns).
    Notification created = 2;
    // Authoritative unread counter. Sent first on every (re)connection and after EVERY change of the counter
    // (a created notification, MarkRead/MarkAllRead from any device), so all devices converge.
    UnreadCountChanged unread_count = 3;
    // Keeps idle proxies and load balancers from closing the stream. Carries no event_id.
    Heartbeat heartbeat = 4;
    // last_event_id is older than the retained backlog: the client must discard it and refetch
    // (GetUnreadCount + ListNotifications). The server then continues with live events.
    ResyncRequired resync_required = 5;
  }
}

message UnreadCountChanged {
  int32 unread = 1;
}

message Heartbeat {}

message ResyncRequired {}
```

`Notification` is the existing message. The RPC is **authenticated** (`AuthRequirement::Required`), scoped to the
caller like every other RPC (`user_id` is never a field), and has no `idempotency_level` (streams are never retried by
the generic client, the desktop supervisor owns reconnection).

## Server behaviour the client relies on

1. **Snapshot first.** Immediately after the stream opens: `unread_count` (so a reconnect heals any missed event).
2. **Resume.** With a valid `last_event_id` the server replays events after it, then continues live. With an unknown or
   expired id it sends `resync_required` then continues live. Retention of a few minutes of backlog per user is enough.
3. **Heartbeat** every 25-30 s. The client treats 90 s of silence as a dead connection and reconnects, so the interval
   must stay well below that and below idle timeouts of the LB/proxy in front (check ALB/Envoy/nginx `idle_timeout`).
4. **Auth.** Validate the bearer token when the stream opens; answer `unauthenticated` (HTTP 401) BEFORE sending any
   frame so the client can refresh the token and reopen. If the token expires while the stream is open, end it with
   `unauthenticated` in the end-of-stream frame; the client refreshes and reconnects.
5. **Draining.** On shutdown send `unavailable` in the end-of-stream frame (not a TCP reset) so clients back off and reconnect to
   another instance.
6. **Fan-out.** The notifications module already consumes integration events (River `InsertTx` jobs) and writes the
   inbox row; the stream can be fed from the same transaction's NOTIFY/pub-sub (for example Postgres `LISTEN/NOTIFY`
   or Valkey pub/sub **as a hint only**; the inbox table stays the source of truth, replay reads it by `event_id`).
   Valkey must not be required for correctness (decisions: Valkey is cache/rate limit only).
7. **Limits.** One stream per client instance; a second `WatchNotifications` from the same user and device may replace
   the first. Cap concurrent streams per user (suggest 5). Streams count against connection limits of the
   HTTP server: `net/http` + h2 handle many idle streams, but set `ReadHeaderTimeout` appropriately and exempt
   this route from the global write timeout.
8. **Preferences.** Server-side preference filtering (`push_enabled`, per-kind switches) already exists for push; the
   stream should apply the same filter to `created` events but always send `unread_count`.

## Compatibility and rollout

- Additive change: new RPC, new messages, no field renumbering. `buf breaking` passes.
- Order: backend ships the RPC (behind a flag if desired) -> desktop `AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS` turns
  on automatically when the proto contains `rpc WatchNotifications` -> desktop switches from polling to streaming per
  session and falls back to polling by itself when the stream is unavailable.
- Until the RPC exists on a deployment, the server returns `unimplemented` (HTTP 404/501): the client detects that,
  degrades to polling and retries the stream hourly, so the order of deployment does not matter.
- Web and mobile clients are unaffected.

## Client readiness (what exists in `apps/desktop`)

- `NotificationRealtime` supervisor: reconnect with exponential backoff and jitter, resume by `last_event_id`, idle
  watchdog, degradation to polling after repeated failures or `unimplemented`, periodic stream retry.
- `ConnectClient` server streaming with envelope framing, token refresh before open and one reopen after `401`.
- Contract entry `kNotificationsWatch` and descriptor tests, compiled only when the proto declares the RPC.
- If the final message or field names differ from this proposal, only
  `Features/Notifications/Data/connect_notification_stream.cpp` and the contract header need adjusting.

## Open questions for the backend team

1. Is `event_id` available (monotonic per user) from the inbox table, for example the notification id as a UUIDv7?
2. Is a retained backlog acceptable, or should `resync_required` be sent on every reconnect (simpler, costs one list call)?
3. Which component owns heartbeats and what are the idle timeouts of the ingress in each environment?
