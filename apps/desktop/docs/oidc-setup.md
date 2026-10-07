# OIDC sign-in: ZITADEL setup and client configuration

Applies when `AEDST_OIDC_ISSUER` and `AEDST_OIDC_CLIENT_ID` are set (ADR-0025). Without them the desktop app uses
the first-party password login.

## 1. ZITADEL

1. Project: the one the API belongs to. Note its **project id** (Project -> General).
2. Application: type **Native** (public client), authentication method **PKCE** (none). Note the **client id**.
3. Redirect URIs: `http://127.0.0.1/callback`. If your ZITADEL instance only matches redirect URIs exactly
   (UNVERIFIED how it treats a variable loopback port), register `http://127.0.0.1:<port>/callback` and start the
   app with `AEDST_OIDC_LOOPBACK_PORT=<port>`. Development mode may be required for `http` redirect URIs.
4. Post-logout redirect URIs: not used (the app does not ask the provider to redirect back after logout).
5. Token settings: **Auth Token Type = JWT** (the API verifies JWT access tokens; the default "Bearer" token is
   opaque), and enable **Add user roles to the access token** or leave the roles scope on (default).
6. Refresh tokens: enable "Refresh Token" in the application's grant types; the client requests `offline_access`.
7. Roles: create the roles the API expects (`admin` ...) in the project and grant them to users.

## 2. Client configuration

| Environment variable | QSettings key | Meaning |
|---|---|---|
| `AEDST_OIDC_ISSUER` | `oidc/issuer` | Issuer URL, e.g. `https://id.example.com` (https required) |
| `AEDST_OIDC_CLIENT_ID` | `oidc/clientId` | Client id of the Native application |
| `AEDST_OIDC_PROJECT_ID` | `oidc/projectId` | Adds scope `urn:zitadel:iam:org:project:id:<id>:aud` |
| `AEDST_OIDC_LOOPBACK_PORT` | `oidc/loopbackPort` | Fixed redirect port (default 0 = OS chooses) |
| `AEDST_OIDC_EXTRA_SCOPES` | `oidc/extraScopes` | Space separated extra scopes |
| `AEDST_OIDC_ROLES_SCOPE` | `oidc/rolesScope` | `0` to stop requesting `urn:zitadel:iam:org:projects:roles` |
| `AEDST_OIDC_END_SESSION` | `oidc/endSession` | `0` to skip opening the provider logout page in the browser |
| `AEDST_OIDC_ROLES_CLAIM` | - | Roles claim read for UI visibility (default `urn:zitadel:iam:org:project:roles` in OIDC mode) |
| `AEDST_ALLOW_INSECURE_LOCALHOST=1` | - | Allows an `http://127.0.0.1` issuer (local development only) |

QSettings is the per-user store written by an installer or a managed-device policy; the environment wins.

## 3. Behaviour to know

- The refresh token is kept in Windows Credential Manager or the Linux Secret Service. Without a Secret Service
  (headless Linux) sign-in fails with a message; nothing is written to disk.
- Starting offline keeps the stored identity; requests refresh the access token once the network is back.
- Signing out revokes the refresh token and opens the provider's logout page in the browser (switch off with
  `AEDST_OIDC_END_SESSION=0`).
- Logs never contain tokens (`aedst.auth` category, redaction is also applied by the logger).
