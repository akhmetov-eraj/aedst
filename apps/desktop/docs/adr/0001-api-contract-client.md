# ADR-0001: API contract client — Connect over Qt Network with generated Protobuf messages

- Status: accepted (implementation not compiled in the authoring environment; see "Verification status")
- Scope: `apps/desktop` (`Aedst::Network`, `Aedst::Contract`)
- Related: [docs/content/library-first.md](../../../../docs/content/library-first.md), server [ADR-0002 ConnectRPC](../../../../docs/content/decisions/0002-connectrpc.md)

## Context

The server (`apps/server`, connect-go) exposes `aedst.auth.v1.AuthService` and
`aedst.tasks.v1.TasksService` over ConnectRPC: the Connect, gRPC and gRPC-Web
protocols, Protobuf payloads. Web (Connect-ES), Apple (connect-swift) and
Android generate typed clients from `apps/server/api/proto`.

Desktop, before this ADR, hand-wrote JSON DTOs and procedure paths
(`dto/*.hpp`, `endpoints.hpp`) on top of `QNetworkAccessManager`. The drift this
invites had already happened: the tree still referenced `UsersService` and
`ProfileService` (removed by server ADR-0007), lacked `Register` /
`GetCurrentUser`, and had nothing for `TasksService`. A CI script
(`check-proto-freshness.sh`) only asked "did someone touch Network/ when the
protos changed", which cannot catch a wrong field name.

Requirements: Qt event-loop integration, TLS/HTTP-2/proxy/OS-certificate
handling that matches the rest of the app, small binary and build, all four
unary+streaming shapes *if* the server ever needs them, and Library-First
compliance (custom code needs a documented technical reason).

## Options

### (a) Official gRPC C++ + protobuf (package manager)

| Aspect | Assessment |
|---|---|
| Event loop | gRPC owns its threads and completion queues / callback executor. Every completion must be marshalled back to the GUI thread (`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`); cancellation and lifetime across two runtimes is where the bugs live. |
| TLS / certificates | Own TLS stack (BoringSSL, bundled) with its own root store. It does not use Schannel / the Windows or system trust store; enterprise TLS-inspection roots and pinned corporate CAs need extra plumbing (`GRPC_DEFAULT_SSL_ROOTS_FILE_PATH`, custom `SslCredentialsOptions`). Qt Network would still be needed for everything else, so two trust configurations to keep aligned. |
| Proxies | Env-var based HTTP CONNECT only; no OS/PAC proxy configuration. |
| Transport | Requires end-to-end HTTP/2 with trailers. Corporate proxies, some CDNs and load balancers break this; Connect unary is plain POST and survives HTTP/1.1. |
| Streaming | All four shapes. |
| Binary / build | Adds gRPC + abseil + re2 + c-ares + upb (+ a second crypto lib). Cold vcpkg build is tens of minutes; binary growth is in the multi-MB range (order-of-magnitude estimate, not measured here). |
| Maintenance | Tight protobuf/abseil/gRPC version coupling; `grpc_cpp_plugin` in the build; frequent API deprecations. |
| Library-First | Best on paper (no custom protocol code), but gRPC C++ is not on the approved desktop stack and *replaces* platform capabilities Qt already provides (HTTP client, TLS, proxy) instead of reusing them. |

### (b) Thin Connect-protocol client on Qt Network + generated Protobuf messages

| Aspect | Assessment |
|---|---|
| Event loop | Native. `QNetworkAccessManager` completes on the owner thread; no threads, no marshalling. |
| TLS / HTTP/2 / proxy / certs | Qt Network: Schannel / Secure Transport / OpenSSL backend, OS trust store, ALPN HTTP/2 with HTTP/1.1 fallback, OS proxy (`QNetworkProxyFactory::setUseSystemConfiguration(true)`), transparent gzip. Nothing reimplemented. |
| Streaming | Unary and server-streaming (Connect envelope framing, implemented and tested). Client- and bidi-streaming are **not** possible: `QNetworkAccessManager` has no full-duplex request bodies. No current RPC needs them. |
| Binary / build | Adds only libprotobuf (+ abseil). `protoc` runs at build time. |
| Maintenance | ~600 lines of protocol code (envelope, error model, client) + tests, against a stable, specified protocol (Connect v1). |
| Library-First | Needs the exception below. |

## Decision

**(b).** Custom code is limited to the Connect wire protocol *on top of* the
platform HTTP client, with these documented technical reasons (Custom Code
Exception):

1. No official Connect client for C++ exists (connectrpc.org ships Go, Swift,
   Kotlin/Java, TypeScript, Python, …; no C++). There is no library to prefer.
2. The only library alternative (gRPC C++) duplicates the HTTP/TLS/proxy/event
   loop that Qt Network already provides and bypasses the OS trust store — a
   security/compliance and operability regression for a desktop app that must
   work behind corporate proxies.
3. Unary Connect is a one-page specification (POST, `application/proto`, JSON
   error body); the risk of owning it is small and is bounded by conformance-style
   tests (below).

Everything that *can* be a library is one: protobuf messages and serialization
(libprotobuf), transport (Qt Network), logging (spdlog), tests (GoogleTest / Qt
Test).

### Wire format

`application/proto` (binary), not JSON: smaller, faster, and — the point of the
change — *generated* from the server protos, so there is no hand-mirrored
schema left to drift. Streaming uses `application/connect+proto` envelopes.

### Code generation

`protoc` from the same package as the linked libprotobuf runs at **build time**
(`cmake/AedstProtobuf.cmake`) over `apps/server/api/proto` (override:
`-DAEDST_PROTO_ROOT`). Why not `buf generate`, nor committed output:

- Generated C++ is only valid against the libprotobuf it was generated for;
  build-time generation with the package's own `protoc` makes skew impossible.
- buf's C++ path (`protoc_builtin: cpp`) shells out to `protoc` anyway and needs
  BSR network access for `buf.build/bufbuild/protovalidate`.
- `buf lint` / `buf breaking` stay where they are (server CI); they check the
  contract, not the C++ output.

The server protos import `buf/validate/validate.proto`; the file pinned by
`apps/server/buf.lock` is vendored in `third_party/protovalidate/` (Apache-2.0)
so builds need neither `buf` nor network, and
`scripts/check-protovalidate-pin.sh` fails CI if it drifts from `buf.lock`. The
desktop compiles it (protoc must resolve the import) but never evaluates the
rules — the server validates every request.

Trade-off accepted: libprotobuf **full** runtime (not `LITE_RUNTIME`) because
`validate.proto` extends `descriptor.proto` options and a lite file cannot import
it; the descriptor-based drift tests below also need reflection. Cost: a larger
binary than lite messages alone. Revisit if the protovalidate import goes away
(e.g. protos moved to `optimize_for = LITE_RUNTIME` and annotations stripped).

### Structure

- `Aedst::Contract` (`Modules/Core/Contract`): the generated messages plus
  `aedst/contract/methods.hpp`, a typed table
  `Method<Req, Res>{path, AuthRequirement, Idempotency}`. Hand-written, but
  guarded: `methods_test.cpp` fails if an entry's path or types disagree with the
  generated descriptors or if the protos gain an RPC without an entry;
  `scripts/check-proto-freshness.sh` says the same in the lint job without a build.
- `Aedst::Network`: `ConnectClient` (typed `unary<Req,Res>` /
  `serverStream<Req,Res>` over an untyped bytes layer), `AuthInterceptor`,
  `ApiError`/Connect error model, envelope codec. Message-type agnostic: depends
  on `MessageLite` only, never on Contract.
- Features depend on Contract + Network and hide them behind their Data-layer
  interfaces (`AuthRemoteDataSource`); Domain still sees `Outcome<T>` /
  `core::Error`.

### Behaviour

- Errors: all 16 Connect codes map to `ApiErrorKind` (the exact code string is
  kept in `ApiError::connectCode`); bodiless failures use the Connect spec's
  HTTP-status table; a 200 that is not `application/proto` (captive portal) is a
  `Decoding` error; redirects are never followed (would replay the POST and the
  bearer token).
- Timeouts: per-attempt total deadline (own timer — `setTransferTimeout` is an
  inactivity timer), also sent as `Connect-Timeout-Ms` so the server cancels work.
- Retries: only methods declared `Idempotency::Idempotent` and only for
  transient errors (`Transport`, `Timeout`, `Unavailable`); exponential backoff
  with equal jitter, default 3 attempts (200 ms → 2 s). Login, RefreshToken and
  every mutation are never retried automatically.
- Auth: `AuthInterceptor` runs per attempt (a retry sees a rotated token) and
  never attaches the bearer to `AuthRequirement::Public` methods.
- Cancellation: `CallHandle::cancel()`; the completion always runs exactly once,
  asynchronously.
- Connection pre-warm: `ConnectClient::prewarm()` opens TCP+TLS(+HTTP/2) before
  the first request.

## Testing / conformance

- GoogleTest against a loopback `FakeHttpServer` (real `QNetworkAccessManager`,
  real generated messages): wire headers and golden request bytes, bearer
  rules, error bodies, retry/no-retry matrix, transport drop, deadline,
  cancellation, non-proto 200, malformed proto, server streaming across chunk
  boundaries, end-of-stream errors, truncated streams.
- Spec vectors: all 16 error codes, the HTTP-status table, envelope framing
  (every split point), end-stream JSON.
- Protobuf golden bytes computed from the encoding rules, unknown-field
  tolerance, well-known types.
- Qt Test benchmarks for the per-call path (`bench` preset).
- **connectrpc conformance suite: not done.** The runner
  (`connectconformance`, Go) drives a client executable over a stdin/stdout
  protocol and needs the conformance protos compiled through the same pipeline.
  It is feasible (a ~200-line adapter using `unaryRaw`/`serverStreamRaw`, run in
  CI next to the Go server) but could not be built or validated here. Until then
  the vectors above are conformance-*style*. Follow-up, in this order: (1) build
  the adapter, (2) run unary + server-stream suites against `ConnectClient`,
  (3) record the expected-failures file for unsupported features (client/bidi
  streaming, GET, per-message compression).

## Revisit triggers

- A feature needs client or bidirectional streaming → gRPC-Web is also HTTP/1.1
  limited; that is the point to reconsider (a), or a WebSocket-based channel.
- The server stops serving the Connect protocol.
- An official Connect C++ client appears → replace the implementation behind the
  same `unary`/`serverStream` API.
- Binary-size budget exceeded by libprotobuf full runtime → see lite trade-off.

## Verification status

Written without a C++/Qt toolchain on the authoring machine (no compiler, CMake
or Qt available): nothing in this ADR's implementation has been compiled or run.
Verified: `apps/desktop/scripts/check-proto-freshness.sh` (13 RPCs match),
`check-protovalidate-pin.sh`, JSON validity of presets/manifest, golden byte
arithmetic. The first `cmake --preset ... && ctest` is the real acceptance test.
