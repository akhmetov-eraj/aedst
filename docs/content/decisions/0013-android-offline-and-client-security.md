# ADR-0013: Android offline writes, session lifecycle and client-side data protection

## Status

Accepted. Client-side counterpart of [ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus) (transactional outbox) and [ADR-0009](/decisions/0009-api-compatibility-and-client-versioning) (client identification).

## Context

The Android template could sign in but not stay signed in (no refresh, no 401 handling), had no way to perform a write while offline, exposed raw server text to users, shipped with backup enabled and no TLS policy, and generated a new device id on every ViewModel instance.

## Decision

**Session lifecycle.** `feature/auth` owns tokens. An expired access token is handled *reactively*: an OkHttp `Authenticator` (`TokenRefreshAuthenticator`, `core/network`) refreshes once on 401 and replays the request; `AuthTokenRefresher` (`feature/auth`) serializes refreshes behind a mutex so a rotating refresh token is redeemed once. A definitively rejected refresh token clears the session; a transient failure keeps it. `SessionState` (`core/common`) is the single signal `app/` routes on, so a background sign-out reaches the login screen from anywhere. The device id is generated once and persisted (`DeviceIdStore`), and deliberately survives sign-out.

**Offline writes.** Local-first outbox: `core/database` holds an `outbox` table, `core/sync` delivers it with WorkManager (`CONNECTED` constraint, exponential backoff, ordered, at-least-once). The outbox row id is the `Idempotency-Key`, so a retry after a lost response is de-duplicated by the server. Handlers are contributed per feature with Hilt `@IntoSet`. `NetworkMonitor` (`core/network`) drives the offline banner. No custom scheduler, no polling, no wake locks.

**Errors.** `AedstError.message` is a diagnostic string for logs. UI shows a localized string chosen by the error *kind* (`AedstError.userMessageRes()`), never the message; analytics record the kind, never the message.

**Client-side data protection.**
- `allowBackup=false`: tokens and the device id are device-bound and useless (or harmful) when restored elsewhere.
- Room is **not encrypted**. Rule: only non-sensitive, re-fetchable data goes in Room; secrets go through `core/security` (Keystore + Tink). This was chosen over SQLCipher to avoid a native dependency, a licence review and a key-management surface for data that should not be there in the first place. `OutboxEntity.payload` must not hold secrets or PII.
- `network_security_config`: HTTPS only, system CAs only; a debug-only overlay allows cleartext to the emulator host and user CAs (debugging proxy).
- TLS pinning from `-Paedst.certPins` (per-environment SPKI hashes, live + backup), applied to the API host with OkHttp `CertificatePinner`. `app:preReleaseBuild` depends on `:core:network:verifyReleaseCertPins`, so a release without pins fails unless `-Paedst.allowUnpinnedRelease=true` is passed deliberately.

**Observability.** Every request carries `X-Aedst-Client: android/<semver>` (ADR-0009) and a W3C `traceparent`, so a client-side failure can be found in the server's OpenTelemetry traces. Debug builds enable StrictMode.

## Consequences

- Features that need offline writes implement one `OutboxHandler` and call `OutboxRepository.enqueue`; they never schedule work themselves.
- Adding a pin rotation is a build-property change, not a code change; the backup pin is mandatory practice.
- Forced-upgrade UX for `CLIENT_UPGRADE_REQUIRED` (ADR-0009) is not implemented on Android yet.

## Revisit when

- Room must hold sensitive data (then SQLCipher, or move that data to the encrypted store).
- A feature needs server push/sync beyond queued writes (add a read-side sync strategy; keep the outbox for writes).
- Pinning causes an outage during a CA change (move to a pin-set with a longer overlap, or pin the intermediate).
