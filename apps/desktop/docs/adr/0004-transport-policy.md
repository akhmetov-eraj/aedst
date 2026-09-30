# ADR-0004: Transport policy — https only, TLS ≥ 1.2, no ignored errors

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Network`, `App/`)

## Decision

`network::validateEndpoint()` / `validateClientConfig()` run at startup and again
inside `ConnectClient`:

- `https` is required. Plain `http` is allowed **only** for loopback hosts
  (`localhost`, `*.localhost`, `127.0.0.0/8`, `::1`) **and** only in Debug builds
  or when `AEDST_ALLOW_INSECURE_LOCALHOST` is `1|true|yes|on`.
- Rejected: empty host, user-info, fragment, query in the base URL, non-http(s)
  schemes.
- Every request carries a `QSslConfiguration` with `TlsV1_2OrLater` and
  `VerifyPeer`. `ignoreSslErrors` is never called; `sslErrors` is logged (error
  text and host only) and the call fails as `Transport` — never retried.
- Optional SPKI SHA-256 pinning (`ClientConfig::pinnedSpkiSha256`, base64) and
  extra CA PEM for corporate roots — both implemented with Qt API only.
- A Release build has **no default API URL**: `AEDST_API_BASE_URL` must be set
  and must be https, otherwise startup fails fast with a logged reason.
- Each call sends `X-Correlation-Id`, reused across retries.

## Consequences

CI tests against the plain-http `FakeHttpServer` set
`allowInsecureLoopback = true` explicitly. Pinning discards the response on
mismatch but cannot guarantee the request was never written (Qt runs HTTP in a
delegate thread).

## Revisit when

The server moves to mutual TLS, or Qt exposes a pre-request handshake callback
that makes pin-before-send possible.
