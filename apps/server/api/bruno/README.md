# AEDST API — Bruno collection

Manual and scripted testing for `apps/server`'s ConnectRPC API. This is the
project's Swagger-UI equivalent — instead of a generated try-it-out page
(ConnectRPC doesn't ship one), this is a real, runnable request collection
covering every public procedure, using [Bruno](https://www.usebruno.com/), a
local-first, git-friendly API client (`.bru` files, no cloud account, no
proprietary export format).

## Getting started

1. Install the [Bruno desktop app](https://www.usebruno.com/downloads) (or use the `bru` CLI — see below).
2. **Open Collection** → select this folder (`apps/server/api/bruno`).
3. Select the **local** environment (top-right dropdown) — it points `{{baseUrl}}` at `http://localhost:8080`.
4. Start the server: `task dev` from `apps/server` (see [`../../README.md`](../../README.md)).
5. (Needs the server started with `AEDST_AUTH_FIRST_PARTY_ENABLED=true`: first-party auth is off by default, otherwise `AuthService` returns UNIMPLEMENTED. With an IdP, set the `accessToken` variable to its bearer token instead.) Run `Auth/Register` first — it creates a throwaway user and captures `userId`/`accessToken`/`refreshToken` as runtime variables that every other request in this collection reuses.

## Layout

```
Auth/                 public — no bearer token needed
  Register              POST AuthService/Register — run this first
  Login                  POST AuthService/Login
  Refresh Token           POST AuthService/RefreshToken
  Logout                   POST AuthService/Logout
  Get Current User          POST AuthService/GetCurrentUser — requires the bearer header itself (set per-request here, not via folder.bru, since Auth/ is otherwise all-public)

Tasks/                 requires Authorization: Bearer {{accessToken}} (folder.bru)
  Create Task
  Get Task
  List Tasks
  Complete Task
  Get Task (Not Found — Wrong Id)   negative test, expects 404 — see its docs block for why 404, not 403
Notifications/         inbox, push-device registry (FCM / Web Push), preferences
Search/                Search (FTS, U+E000/U+E001 highlight markers), Suggest
Media/                 Create Upload -> PUT to the presigned URL yourself -> Complete Upload -> Get/List/Delete
Admin/                 needs the `admin` role (built-in policy /aedst.admin.v1.AdminService/*=admin)
```

The collection sends `Connect-Protocol-Version: 1` on every request
(collection.bru): the server rejects Connect calls without it (CSRF defense in
depth, `AEDST_REQUIRE_CONNECT_PROTOCOL_HEADER`).

(`Users/` and `Profile/` were removed along with those modules — see
[ADR-0007](../../../../docs/content/decisions/0007-merge-users-into-auth.md).)

Every request's own `docs` block explains its preconditions and, for the
negative tests, exactly which server-side check it's exercising and where
that check lives in the Go source.

## Why the auth token is a plain header, not Bruno's Bearer auth block

`Tasks/folder.bru` sets a literal
`Authorization: Bearer {{accessToken}}` header rather than using Bruno's
structured `auth:bearer` block. This matches exactly what the server expects
(`internal/platform/httpserver/interceptor.go` reads the raw `Authorization`
header) and is the same approach `apps/apple`'s `AuthInterceptor.swift`
takes. `{{accessToken}}` is a **runtime variable**, set by `Register`'s and
`Login`'s `script:post-response` blocks (`bru.setVar(...)`) — it is never
stored in the `local` environment file, so it never ends up in git history.

## Re-running

`collection.bru`'s `script:pre-request` generates one `runId` (a timestamp)
the first time any request in a run executes, and every request that needs a
unique email/deviceId reuses it — so re-running `Register` in the same
Bruno session after the app restarts (which clears runtime variables) gets a
fresh, collision-free user instead of failing with `already_exists`.

## Running headless (CLI / future CI)

```bash
npx @usebruno/cli run apps/server/api/bruno --env local -r
```

`-r` runs folders in their `seq` order (Auth → Tasks), so
`Register` populates the shared variables before anything that needs them
runs. Not wired into `.github/workflows/server-ci.yml` yet — doing so needs
the full `docker compose` stack (Postgres/Valkey) plus a running
`aedst-server` up first, the same prerequisite `task test:integration`
already has locally but that CI doesn't run either; a natural follow-up
once that gap is closed, not scope-crept into this collection's setup.
