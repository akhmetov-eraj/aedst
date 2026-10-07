# ADR-0009: API Compatibility and Client Versioning

## Status
Accepted (contract rules and client header: to be implemented; per-app build
scaling: recommendations only, see the last section)

## Context
Five client platforms (Web, Android, Apple, Desktop) call one API
([ADR-0002](/decisions/0002-connectrpc)). Mobile and desktop binaries stay in the
wild for months after a release, so the server cannot assume all clients are
current, and cannot remove or change anything a shipped client still relies on.
We need explicit rules for evolving the contract, a way to know who is calling,
and a way to retire clients that are too old.

## Decision

### Proto contract rules
- **Additive-only within `v1`**: new fields, new RPCs and new enum values are
  allowed; removing or renaming fields, changing types or changing semantics is not.
- **Never reuse a field number.** When a field is removed, its number and name are
  marked `reserved`.
- **Deprecate before removing**: mark with `[deprecated = true]` and keep the field
  working for N releases (N = the support window below).
- Clients must ignore unknown fields and treat unknown enum values as
  `UNSPECIFIED`-like (never crash).
- **`buf breaking` with `FILE` mode is the gate**: it runs in CI against `main`
  (`task breaking`) and a failure blocks the merge. A genuine breaking change is a
  new API version (below), not an override.

### Client identification
Every first-party client sends `X-Aedst-Client: <platform>/<semver>` on every
request, where platform is one of `web | android | apple | desktop`, for example
`X-Aedst-Client: android/1.4.2`. It is used for metrics (requests by client
version), logs and the version gate; it is **not** authentication and is never
trusted for authorisation.

### Minimum supported version gate
- The server reads `AEDST_MIN_CLIENT_VERSIONS`, format
  `web=1.0.0,android=1.2.0` (empty = gate disabled).
- If the header names a platform with a configured minimum and the version is lower,
  the request is rejected with Connect code `failed_precondition` and an error
  detail with reason `CLIENT_UPGRADE_REQUIRED` (domain `aedst`), so the client can
  show a forced-upgrade screen instead of a generic error.
- A **missing or unparsable header is allowed** (curl, tests, older clients that
  predate this ADR); the gate only acts on clients that identify themselves.
- The health endpoints (`/livez`, `/readyz`) are plain HTTP outside the RPC stack and
  are never gated.
- The value is plain configuration (Helm `config.minClientVersions`), so raising the
  minimum is a config rollout, not a code release.

### Support window
- **Mobile and desktop**: the last 2 minor releases, or 6 months, whichever is
  longer. Raise the minimum only after telemetry shows the remaining share of
  older versions is negligible, and after a deprecation notice has shipped in the
  app.
- **Web**: always latest; the bundle is served from the deployment and stale tabs
  are covered by the same forced-upgrade path (reload).

### Introducing v2
A breaking change creates a new proto package, for example `aedst.tasks.v2`, served
by the same binary **in parallel** with `v1` (separate ConnectRPC handlers, shared
domain and application layers). `v1` is deprecated, kept for the support window and
removed only when the gate/telemetry show no remaining callers. Only the transport
layer knows about the version; the domain does not.

### Per-app build scaling (recommendations, not implemented)
Current state, verified in `.github/workflows`: CI is already **path-filtered per
app** (`server-ci`, `web-ci`, `android-ci`, `apple-ci`, `desktop-ci`, `docs-ci`),
and the desktop workflow additionally triggers on `apps/server/api/proto/**`.

When the feature count approaches ~10, the following are recommended; none is
implemented today:
- **Android**: enable the Gradle build cache and configuration cache
  (`org.gradle.caching=true`, `org.gradle.configuration-cache=true`), with a remote
  cache in CI.
- **Apple**: Tuist binary cache (`tuist cache`) so unchanged feature modules are
  not recompiled.
- **Desktop**: CMake with ccache and vcpkg binary caching in CI.
- **Web**: pnpm store caching now; Turborepo-style task caching only if web grows
  into multiple packages.
- Keep modules independent so each platform's cache hits stay high (see the
  architecture pages of each client).

## Consequences
- Old clients keep working as long as the additive-only rules hold, and `buf
  breaking` makes accidental breaks a CI failure.
- Forcing an upgrade becomes an operational lever with a clear client-side UX.
- Every client must send the header and implement the upgrade screen; until they do,
  the gate has no effect on them.
- Running `v1` and `v2` together costs extra code and tests for the duration of the
  support window.
- Support windows are a product commitment: shortening them requires an explicit
  decision.
