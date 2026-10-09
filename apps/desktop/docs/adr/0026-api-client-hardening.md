# ADR-0026: API client hardening - session refresh, caller-bound cancellation, retry rules

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Network`, `Aedst::Oidc`, `App/`)
- Builds on: ADR-0001 (Connect client over Qt Network), ADR-0025 (OIDC)

## Context

The client already had per-attempt deadlines, jittered retries for idempotent methods, cancellation handles,
the full Connect error mapping to `ApiError` (`std::expected<T, ApiError>`), server streaming and a descriptor
drift test for `contract/methods.hpp`. Missing: the access token was never refreshed after startup (a signed-in
user was sent to login after the token TTL, about 15 minutes), a call could not be tied to the lifetime of its
caller, and retries also covered `resource_exhausted`.

## Decisions

1. **Refresh in the interceptor, one mechanism for every request.** `AuthInterceptor` takes an optional
   `TokenRefresher` (implemented by `oidc::OidcSession`):
   - `prepare()` runs before every attempt of an authenticated unary or stream call; a token that is about to
     expire is refreshed first. Only a *finished* session (`Unauthenticated`) fails the call, without sending
     anything; a transient failure lets the request go out with the current token.
   - After an `unauthenticated` answer the token is refreshed **once** and the call replayed **once**
     (`refreshAfterRejection`, which returns the current token without a request if another caller already
     refreshed). The replay keeps its attempt number, so the retry budget is untouched. It is safe for
     non-idempotent methods because the server rejected the request before running the handler. A second
     rejection is final (no loops).
   - If the refresh fails because of the network the call completes with `Transport`, **not** `Unauthenticated`,
     so `ErrorCenter` does not sign the user out while offline.
   - Single flight lives in `OidcSession` (ADR-0025): N concurrent callers, one token request.
   - Public methods are never refreshed or replayed. Streams are covered the same way (refresh before open,
     one reopen after a 401 at open; never after messages were delivered).
2. **Retry rules.** Unchanged for the schedule (exponential, equal jitter, idempotent methods only, TLS
   failures never). `ApiError::isRetryable()` now excludes an explicit `resource_exhausted` code; `unavailable`
   (including 502/503/504 and bodiless 429), `deadline_exceeded` and connection-level failures (which Connect
   defines as `unavailable` on the client) are retried.
3. **Caller-bound cancellation.** `CallHandle::cancelSilently()` cancels without running the completion.
   `ConnectClient::unary/serverStream(..., const QObject* context, ...)` cancel silently when the context is
   destroyed; `CallScope` (a member of a short-lived caller) cancels everything it holds in its destructor.
   Silent, because a normal `Cancelled` completion would run into a half-destroyed owner. Adoption in existing
   Data repositories is not done: they are long-lived singletons and a `CallHandle` cannot cross the Qt-free
   Domain boundary; use `CallScope`/context overloads in new QObject callers that start calls directly.
4. **No exception across Qt slots.** Every call into caller code (call completion, stream completion, stream
   message handler) goes through `invokeGuarded`, which logs and swallows. Qt terminates the process if an
   exception leaves a slot.
5. **Contract tests.** The existing Contract tests are the equivalent of web `pnpm test:contract`:
   `ctest -R AedstContract` checks every table entry against the protoc descriptors (request/response types,
   streaming flags, idempotency), every proto RPC against the table, and the public-procedure set against the
   server. `scripts/check-proto-freshness.sh` runs the RPC-name part without a build. Phase 6 adds the streaming
   RPC entries to them.
6. **Qt GRPC.** Not adopted; [qt-grpc-evaluation.md](../qt-grpc-evaluation.md) states what a move would bring and the
   decision it needs (commercial Qt license).

## Consequences

- The 15-minute sign-out is fixed for OIDC mode. First-party password mode has no refresher; it keeps its old
  behaviour until its sunset.
- A replayed non-idempotent call is possible only after an `unauthenticated` rejection.
- New tests: `Modules/Core/Network/tests/connect_client_auth_test.cpp` (pre-flight, replay-once, no loops,
  offline refresh failure, retry budget, cancel during refresh, streams, context/scope cancellation, throwing
  callbacks) and an extended `api_error_test.cpp`. None has been compiled yet.

## Revisit when

Protos declare `idempotency_level` (then derive the method table from them), the server stops rejecting before
the handler runs for `unauthenticated`, or a commercial Qt license makes Qt GRPC an option.

## Proposal awaiting the owner: `google.rpc.ErrorInfo` through generated messages (nothing implemented)

`connect_error.cpp` reads the `reason` of the server's `google.rpc.ErrorInfo` detail (needed for `CLIENT_UPGRADE_REQUIRED`,
ADR-0034) with a small hand-written parser of the Protobuf wire format. It is covered by the replay fixtures, but it is custom
code for something a library does: the buf/protoc toolchain can generate `google.rpc.ErrorInfo` (and `Status`) like every other
message.

**Recommendation: replace it** with the generated `google::rpc::ErrorInfo` (from `googleapis`' `error_details.proto`, vendored
under `third_party/` and pinned like `protovalidate`), decoded from the base64 `details[].value` that the Connect error body
carries, and keep `error_kinds_replay_test` / `client_version_gate_test` as the safety net. **Why not now:** it changes the error
model that every test replays and adds a vendored proto; it is best done together with the first real build and compiler.
