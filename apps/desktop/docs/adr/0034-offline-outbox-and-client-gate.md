# ADR-0034: Offline outbox, Idempotency-Key, client identification and the forced-upgrade screen

- Status: accepted; **not compiled or run** (no Qt/CMake toolchain in the authoring environment). The tests below are written, not executed.
- Scope: `Modules/Core/Outbox`, `Modules/Core/Persistence` (migration 2), `Modules/Core/Network` (`CallOptions::idempotencyKey`,
  `ClientConfig::clientIdentifier`, `ApiError::reason`), `Modules/Core/Navigation` (`Router::lockTo`),
  `Modules/Features/Tasks` (handlers, pending indicator), `App/` (wiring, `UpgradeRequiredView.qml`).
- Counterpart of Android's [ADR-0013](/decisions/0013-android-offline-and-client-security) and the server's
  [ADR-0009](/decisions/0009-api-compatibility-and-client-versioning). Behaviour is pinned by the shared fixtures
  `docs/contract/behavior/{offline-outbox,session-refresh,client-version-gate}.json`.

## Decisions

1. **Outbox table in the existing database.** Migration 2 (`create_outbox`) in `Core/Persistence`'s single migration list: `seq`
   (delivery order), `id` UNIQUE (the Idempotency-Key; `INSERT OR IGNORE` makes a double enqueue a no-op), `type`, `payload`,
   `attempts`, `status` (0 pending, 1 parked), `last_error` (an error KIND, never a message). It runs on the same `DatabaseWorker`
   thread as the token store, which is now created for both the first-party and the OIDC sign-in mode.
2. **`Aedst::Outbox` (Core).** `OutboxQueue` is the asynchronous persistence port (`SqliteOutboxQueue` on the worker);
   `OutboxDrainer` is the framework-free FIFO state machine; `OutboxService` is the QObject that triggers it. No new dependency:
   SQLite, `RetryPolicy` (shared exponential backoff with equal jitter) and Qt only.
3. **Delivery rules (identical to the fixture and to Android):** items are delivered in creation order; the item id is the
   `Idempotency-Key` of every attempt; success removes the item; a retryable failure records an attempt and **stops the run**; a
   permanent failure, an item type nobody handles, a handler that throws, or the 8th failed attempt **parks** the item and the run
   continues. One addition: a handler can answer `Blocked` (`unauthenticated`, `upgrade_required`), which stops the run without
   counting an attempt, so a signed-out or outdated client does not burn the retry budget of the user's queued work.
4. **Triggers.** After `enqueue`, at start/resume of a session, when `ConnectivityMonitor` reports online (wired in `main.cpp`, so
   Outbox does not depend on Connectivity), and after a retryable failure with backoff 2 s, doubling, capped at 5 min. Nothing runs
   while offline; one run at a time, a trigger during a run schedules exactly one more.
5. **Handlers are contributed per feature through the plugin `ServiceRegistry`.** The host provides `OutboxHandlers` and the
   `ConnectClient` before `PluginManager::activate()`; `TasksPlugin::registerServices` adds `task.create` and `task.update`. Without
   them the feature works online only. (This is why plugin activation and the Router now come after the API client in
   `AppDependencies::create`.)
6. **Tasks.** The create and the complete/reopen call are made directly first, with a fresh Idempotency-Key. Only a
   `Network`-class failure falls back to the outbox, **with the same key**, so a request whose response was lost is applied once. The
   row is shown as pending ("Pending sync" badge, "Waiting to sync: N" banner) until the queue is empty, then the list reloads.
   `task.update` carries `{id, done}` (complete/reopen is the only edit the desktop UI offers; there is no UpdateTask call site yet).
7. **Client identification.** `ClientConfig::clientIdentifier` (`desktop/<semver>` from `AEDST_VERSION`, omitted when it is not
   semver) is set on every unary and streaming request (`X-Aedst-Client`). OIDC token-endpoint and presigned-upload requests are
   not API calls and do not carry it.
8. **Forced upgrade.** `ConnectError` parses the `google.rpc.ErrorInfo` detail (its `debug.reason` or, when absent, the known reason
   inside the decoded proto bytes); `failed_precondition` + `CLIENT_UPGRADE_REQUIRED` becomes `ApiError::isUpgradeRequired()`,
   `core::ErrorCode::UpgradeRequired` and the `ConnectClient::clientUpgradeRequired()` signal. `AppDependencies` answers by
   `Router::lockTo(app/upgrade-required)`: the router then refuses every later push/reset/back, so no late sign-in or restore can
   navigate away. `UpgradeRequiredView.qml` points at the existing update flow (`UpdateService::checkNow` / `installAndRestart`).
   `ErrorCenter` does not toast it.
9. **Sign-out purges the outbox** (and so does an interactive sign-in), so queued writes are never delivered under another account.

## Security and privacy notes

- The `outbox` table is **not encrypted** (like Android's Room). A payload is only what the operation needs: for tasks the title,
  description, priority and due date the user typed. No tokens, no e-mail, no ids other than the task id. Anything sensitive must not
  be queued; if that ever changes, encrypt the payload with `SecureStorage` (AES-GCM, already in Persistence) first.
- Queued writes that are still pending when the user signs out are **discarded**, not carried to the next login.

## Known gaps / revisit when

- Nothing was compiled or run. The first CI build is the acceptance test (including the Windows-only path define for the fixtures).
- A pending create has a local-only id (`local:<uuid>`); it cannot be opened or completed until the server confirms it.
- Server streaming calls are not gated by the server and are not wired to the upgrade signal.
- Revisit when an `UpdateTask` UI exists (extend `task.update` with a field mask and `expected_version`; map `aborted` to a conflict
  screen instead of parking).
