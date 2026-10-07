# ADR-0001: Identity provider and OIDC resource-server auth

- Status: Accepted
- Date: 2026-09-29
- Scope: `apps/server` (clients are unchanged; see "Client impact")

## Context

The server shipped with self-built authentication: an `auth` module that
stores password hashes, issues its own HS256 access/refresh JWTs and rotates
sessions. That code works, but **an identity system is the highest-liability
component a boilerplate can own**: password storage, account recovery, MFA,
passkeys, social login, email verification, brute-force defence, session
management, audit and compliance (GDPR/SOC 2) are a product in themselves, and
every project stamped out of this template would inherit the maintenance and
the risk.

The template is meant for *any* kind of project, so the decision must not
hard-wire a vendor into the server.

## Decision

1. **The server becomes a standards-based OIDC resource server.** It verifies
   access tokens from *any* OpenID Connect provider (`internal/platform/auth`,
   `OIDCVerifier`): discovery from `<issuer>/.well-known/openid-configuration`,
   JWKS fetched and refreshed in the background (`jwkfetch`, jwx v4), issuer and
   audience validated, asymmetric-only algorithm allow-list, one rate-limited
   forced refresh on an unknown `kid` (key rotation). Everything is
   configuration (`AEDST_OIDC_*`); there is no provider-specific code, so
   switching provider is an environment change.
2. **A `Verifier` interface is the only thing the request path knows.**
   `MultiVerifier` routes a token to first-party or OIDC verification by its
   `iss`; the chosen verifier still fully validates it.
3. **The first-party auth module stays, behind the same contract, and is
   optional** (`AEDST_AUTH_FIRST_PARTY_ENABLED`). Existing clients keep using
   `AuthService` unchanged. New projects are expected to run with an external
   IdP and switch the module off.
4. **Recommended IdP: Keycloak** as the template default (dev profile included),
   with **Zitadel** as the documented alternative. Ory Kratos+Hydra is not
   recommended for this template.
5. **Harden what remains** of first-party auth (argon2id, atomic refresh
   rotation with reuse detection, per-IP and per-account rate limits,
   timing-equalised login) until a project migrates off it.

## Options considered

Facts verified against the projects' repositories on 2026-09-29.

| | Keycloak | Zitadel | Ory Kratos + Hydra |
|---|---|---|---|
| License | **Apache-2.0** (CNCF incubating) | **AGPL-3.0** for core, console and hosted login since v3 (2025-03-31); v2 and earlier were Apache-2.0; proto definitions, SDKs, APIs, examples, docs, Helm chart stay Apache-2.0 | **Apache-2.0** (both) |
| Latest release seen | 26.7.4 (2026-09-16) | v4.19.2 (2026-09-28) | n/a |
| Runtime | JVM; ~0.5-1 GB RAM baseline; PostgreSQL | Single Go binary + a small Next.js login UI container; PostgreSQL only; light | Two Go services (+ Oathkeeper/Keto if you want proxy/authz); PostgreSQL |
| What you build | Almost nothing; theming via FreeMarker/Keycloakify | Almost nothing; login UI v2 is a separate deployable | **Everything user-facing**: login, registration, recovery, consent UIs are yours |
| Standards | OIDC (certified), OAuth 2.x, SAML 2, LDAP/AD federation, fine-grained authz | OIDC (certified), OAuth 2.x, SAML, passkeys/WebAuthn, multi-tenancy (orgs) natively | OIDC/OAuth 2 (Hydra), identity/self-service flows (Kratos) |
| Ecosystem / hiring | Largest by far; every language has adapters and tutorials | Growing; smaller | Mid; high integration cost |
| Managed option | Several vendors | Zitadel Cloud | Ory Network |

### Why Keycloak is the template default

- **Zero license friction.** Apache-2.0 has no copyleft or network-use clause.
  Projects derived from this template may be commercial, closed, or subject to a
  corporate policy that bans AGPL; a template must not make that decision for
  them.
- **Lowest ops risk through familiarity.** It is the de-facto standard; on-call
  engineers, auditors and clients already know it. The dev profile is one
  container (`start-dev`), so it stays easy to keep working.
- **Feature ceiling.** SAML brokering, LDAP/AD federation and enterprise SSO —
  the things a B2B customer eventually demands — are built in.
- Cost: heavier footprint (JVM) and a bigger admin surface than Zitadel.

### When to choose Zitadel instead

Pick it when the AGPL-3.0 is acceptable (using it *unmodified as a separate
service* imposes no obligations on the application code; the copyleft applies
to Zitadel itself if you modify and offer it) and you value the smaller
footprint, PostgreSQL-only operations, native multi-tenancy (organisations) or
the hosted Zitadel Cloud. Nothing in the server changes: set
`AEDST_OIDC_ISSUER`, `AEDST_OIDC_AUDIENCE` and
`AEDST_OIDC_ROLES_CLAIM=urn:zitadel:iam:org:project:roles` (an object keyed by
role name, which the verifier understands). Zitadel subjects are numeric, which
the verifier maps deterministically (see below).

### Why not Ory Kratos + Hydra

Excellent, permissive and light, but *headless*: the template would have to ship
and maintain login/registration/recovery/consent UIs for four client platforms.
That re-creates the liability this ADR exists to remove. Reasonable for a
project that needs a fully custom identity UX; not a good default.

### Why not "keep self-built auth as the default"

Kept only as a compatibility path. Owning credential storage plus session
management is exactly the risk described in Context, and the template should
not encourage new projects to start there.

## Server design

- **Identity mapping.** Modules key data by `ids.ID` (UUID). A token subject that
  is a UUID (Keycloak, Cognito) is used as-is; any other subject (Zitadel
  numeric ids, `auth0|...`) maps to a deterministic ID derived from
  `(issuer, subject)` (`ids.FromExternal`). No users table is needed to start; a
  project that needs profiles adds a table keyed by that ID.
- **Roles.** `AEDST_OIDC_ROLES_CLAIM` accepts a claim name or a dotted path
  (`realm_access.roles` for Keycloak); arrays, role-keyed objects and
  space-separated strings are understood.
- **Fail closed.** If discovery or the JWKS is unreachable the request is
  rejected (401) and retried with a 5-second back-off; the server still boots
  and first-party tokens keep working.
- **Config validation** refuses an issuer without an audience, plain `http`
  issuers outside development, and any `HS*`/`none` algorithm for external
  issuers (algorithm-confusion guard).
- **Not done here (intentionally).** Token revocation/introspection (RFC 7662)
  and the browser/mobile login flows are the IdP's job; clients that adopt the
  IdP use its Authorization Code + PKCE flow directly.

## Client impact

None required. Web, Android, Apple and Desktop keep calling `AuthService`
(register/login/refresh/logout) exactly as before; the proto changed only by
adding `max_len` validation constraints (wire-compatible). Consequences of the
hardening for existing users:

- Existing bcrypt password hashes keep working and are **upgraded to argon2id
  transparently on the next successful login**.
- Access/refresh tokens issued before deploy remain valid (no claim the old
  tokens lack is required).
- A client that refreshes concurrently with the *same* refresh token from two
  places (e.g. two tabs racing) will now have its session revoked by reuse
  detection; clients must serialise refreshes.

## First-party auth hardening (what changed)

| Gap found | Fix |
|---|---|
| bcrypt default cost, silent 72-byte truncation | argon2id (RFC 9106 second recommendation: t=3, m=64 MiB, p=4), max password 128 bytes, bounded concurrency (8) so hashing cannot exhaust memory; bcrypt verified and upgraded on login |
| Login timing revealed which emails exist | Unknown accounts burn an equivalent dummy hash |
| Refresh rotation was read-then-write (race: two requests both succeeded) | Atomic compare-and-swap on the stored hash; loser revokes the session |
| Refresh tokens issued in the same second were byte-identical (reuse undetectable) | `jti` + `nbf` claims on every token |
| No throttling | Per-IP limit on register/login/refresh RPCs and per-account limit on login (Valkey, cluster-wide, fail-open) |
| Emails case-sensitive (duplicate accounts) | Normalised to lower case |
| Proto validation annotations were never enforced | `connectrpc.com/validate` interceptor; length caps added |
| Dev signing key accepted anywhere; no minimum length | Config rejects keys < 32 bytes, and the dev placeholder in staging/production |

Already correct and kept: constant-time hash comparison, refresh tokens stored
only as SHA-256 digests, server-side session revocation.

## Consequences

- New projects: run Keycloak (or Zitadel), set `AEDST_OIDC_*`, set
  `AEDST_AUTH_FIRST_PARTY_ENABLED=false`, delete the `auth` module when ready.
- The dev compose profile `idp` provides a pre-configured Keycloak realm
  (`deployments/keycloak/aedst-realm.json`, user `dev`/`dev`, audience mapper for
  `aedst-api`). It is dev-only (H2, HTTP, throwaway credentials).
- The Zitadel official compose stack is four containers (proxy, API, login UI,
  Postgres); it is deliberately not vendored here — see the Zitadel docs if you
  choose it.
- Revisit when: a second first-party consumer needs SSO across apps, or the
  built-in module is deleted (then remove `TokenService`, the auth module, and
  `AEDST_JWT_*`).

## Verifying against a real provider

```bash
docker compose -f deployments/docker/docker-compose.yml --profile idp up -d --wait
export AEDST_OIDC_ISSUER=http://localhost:8082/realms/aedst
export AEDST_OIDC_AUDIENCE=aedst-api
export AEDST_OIDC_ROLES_CLAIM=realm_access.roles
TOKEN=$(curl -s -d grant_type=password -d client_id=aedst-web -d username=dev -d password=dev \
  "$AEDST_OIDC_ISSUER/protocol/openid-connect/token" | jq -r .access_token)
curl -s -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{}' \
  http://localhost:8080/aedst.tasks.v1.TasksService/ListTasks
```
