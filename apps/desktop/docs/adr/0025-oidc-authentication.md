# ADR-0025: Sign-in with ZITADEL (OpenID Connect, Authorization Code + PKCE)

- Status: accepted (not compiled in the authoring environment - no Qt/vcpkg toolchain)
- Scope: `apps/desktop` (`Aedst::Oidc`, `Features/Auth`, `App/`)
- Implements: server ADR-0001 (`apps/server/docs/adr/0001-identity-provider.md`): Desktop moves to Authorization Code + PKCE before `AEDST_AUTH_FIRST_PARTY_SUNSET`
- Supersedes in part: ADR-0003 (the SQLite `auth_session` token table is no longer the session store in OIDC mode)

## Context

Desktop used e-mail and password against the first-party `AuthService.Login`. The server is now an OIDC
resource server and ZITADEL is the default identity provider. The desktop client must obtain tokens as a
public native client: system browser, PKCE, loopback redirect, no embedded web view, no secret in the app.

## Decision

New Core module `Aedst::Oidc` (Qt Core/Network/Gui, **QtNetworkAuth**, **QtKeychain**) and an
`OidcAuthRepository` behind the existing `AuthRepository` port.

**Flow.** Discovery (`/.well-known/openid-configuration`, issuer must equal the configured issuer, PKCE S256
must be advertised when the provider lists methods, endpoints must be https) -> `QOAuth2AuthorizationCodeFlow`
with `PkceMethod::S256` and `QOAuthHttpServerReplyHandler` on `127.0.0.1`, port chosen by the OS (RFC 8252
section 7.3), callback path `/callback` -> `authorizeWithBrowser` opens the default browser
(`QDesktopServices`) -> code exchange -> ID token checks (`iss`, `aud`/`azp`, `exp`, `iat`, `sub`; no signature
check because the token comes straight from the token endpoint over TLS, OIDC Core 3.1.3.7) -> refresh token
stored in the OS keychain.

**Scopes.** `openid profile email offline_access`, plus the ZITADEL audience scope
`urn:zitadel:iam:org:project:id:<projectId>:aud` and (default on) `urn:zitadel:iam:org:projects:roles`.
Issuer, client id and project id are configuration (`AEDST_OIDC_ISSUER`, `AEDST_OIDC_CLIENT_ID`,
`AEDST_OIDC_PROJECT_ID`, or QSettings `oidc/*`), never constants. OIDC is active exactly when issuer and
client id are set; otherwise the app keeps the first-party password login until its sunset.

**Token storage.** Only the refresh token and the subject are persisted, through QtKeychain
(Windows Credential Manager; Linux Secret Service/libsecret or KWallet), entry name
`oidc-<24 hex of SHA-256(issuer\nclientId)>`, service `com.aedst.Desktop`. Access and ID tokens live in
memory only (wiped `SecretString`s). If the keychain is unavailable (typically headless Linux) sign-in
**fails with an explicit error** and the freshly issued refresh token is revoked; there is **no file
fallback** (`setInsecureFallback(false)`). This is stricter than the legacy `SecureStorage`, which still
falls back to a `0600` key file for the first-party session until Phase 7.

**Refresh.** `OidcSession` owns a single-flight refresh: any number of callers of `ensureFreshAccessToken` /
`refreshAfterRejection` share one token request. Network failures keep the session (`ErrorCode::Unavailable`, renamed from `Network` to the shared error-kinds vocabulary);
`invalid_grant`/`invalid_token`/`invalid_client`/`unauthorized_client` end it (`sessionExpired()`, keychain
cleared). A refresh result that arrives after `signOut()` is discarded (epoch fence) so it cannot resurrect
the session. A rotated refresh token is persisted. Wiring the interceptor (refresh before sending, refresh
once on `unauthenticated`) is Phase 5.

**Logout.** Local state and keychain are cleared first and the caller is told immediately; then the refresh
token is revoked (RFC 7009, `revocation_endpoint`, which also revokes the access token in ZITADEL) and, if
`end_session_endpoint` exists and `endSessionInBrowser` is on, the browser is sent to it with `id_token_hint`
and `client_id`. Provider-side failures are logged, not surfaced.

**UI.** `LoginViewModel` gained `browserSignInAvailable`, `waitingForBrowser`, `restoringSession`,
`signInWithBrowser()`, `cancelBrowserSignIn()`. `LoginView.qml` shows a single "Continue in browser" button
(with cancel and a hint) instead of the e-mail/password form when OIDC is on, and a progress indicator while
the stored session is restored at startup. A user cancelling the browser step is not shown as an error.

**Roles.** In OIDC mode the Admin UI reads ZITADEL's `urn:zitadel:iam:org:project:roles` claim (object whose
keys are role names; the existing parser already supports that shape). Override with `AEDST_OIDC_ROLES_CLAIM`.
The claim is read unverified for UI visibility only; the server enforces roles.

## Dependencies (Library-First)

- **QtNetworkAuth** - Qt add-on module; the standard-library/platform step of the order of preference.
  Requires the `qtnetworkauth` module in the Qt install (CI, `BUILDING.md`).
- **QtKeychain 0.17.0** - BSD-3-Clause, Qt 6 by default, released 2026-07-06 (checked via the GitHub releases
  API and tag `0.17.0`, tag object `85835a638c6598ecd712473c555360a29746aad1`). Qt has no credential-store
  API, hand-written DPAPI/libsecret/KWallet code would be exactly what Library-First forbids. Built from
  source with `FetchContent` (static, `EXCLUDE_FROM_ALL`, see `cmake/AedstQtKeychain.cmake`), not via vcpkg,
  because the vcpkg port would bring its own Qt. Written down in `docs/content/library-first.md`.
- Custom code with a documented reason: token revocation (RFC 7009) is a ~20-line form POST because
  QtNetworkAuth has no revocation API; ID-token claim checks are project-specific validation.

## UNVERIFIED (confirm on the first real run)

1. QtNetworkAuth 6.12 behaviour is coded against the documented 6.9+ API (`setTokenUrl`,
   `setRequestedScopeTokens`, `setPkceMethod`, `refreshTokens`, `idToken`, `serverReportedErrorOccurred`).
   That a **refresh** ends with `statusChanged(Granted)` and a rotated `refreshToken()` is inferred, not
   observed; the code also settles on `requestFailed`, `serverReportedErrorOccurred` and a timeout, so a
   different signal order degrades to a network error instead of hanging.
2. Whether ZITADEL accepts a loopback redirect with a **random port**. RFC 8252 requires it; ZITADEL's
   documentation did not state it. If it matches redirect URIs exactly, register
   `http://127.0.0.1:<port>/callback` and set `AEDST_OIDC_LOOPBACK_PORT=<port>`.
3. ZITADEL scope names (`urn:zitadel:iam:org:projects:roles`) and that the application is set up as
   "Native" with PKCE and **JWT access tokens** (the server resource server verifies JWTs; ZITADEL issues
   opaque "Bearer" tokens by default). See `docs/oidc-setup.md`.
4. That QtKeychain's CMake target is `qt6keychain` and that `keychain.h`/`qkeychain_export.h` resolve from the
   source/binary directories as `AedstQtKeychain.cmake` assumes.
5. The Flatpak runtime ships QtNetworkAuth; FetchContent does not work in the Flatpak sandbox (no network at
   build time): the manifest needs a `qtkeychain` module with a pinned source (Phase 8).

## Consequences

- Passwords never pass through the desktop app in OIDC mode.
- The first-party code paths stay until `AEDST_AUTH_FIRST_PARTY_SUNSET`; removing them (and the
  `auth_session` table) is a follow-up, together with Phase 7.
- Headless Linux without a Secret Service cannot sign in; this is deliberate.

## Revisit when

ZITADEL (or the chosen IdP) cannot do loopback with variable ports and a fixed port is not acceptable, QtNetworkAuth
gains revocation, or the first-party login is removed.
