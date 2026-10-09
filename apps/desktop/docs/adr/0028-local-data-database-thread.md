# ADR-0028: Local data - database thread, schema history, and why there is no SQLCipher yet

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Persistence`, `Features/Auth` data layer, `App/`)
- Supersedes in part: ADR-0005 (the "GUI-thread budget" decision; schema versioning and deletion rules stand)
- Superseded in part by: ADR-0034 (the outbox and `kv` tables: the local database now also holds queued writes; see "Later changes")

## Context

Phase 7 asked: (1) is sensitive data stored locally, and if so should the database be encrypted; (2) versioned
migrations, WAL, foreign keys, busy timeout; (3) no database work on the GUI thread; (4) migration tests from every
previous schema version.

## What is stored locally (the inventory)

| Where | What | Sensitive | Protection |
|---|---|---|---|
| OS credential store (OIDC mode, ADR-0025) | refresh token + subject | yes | Credential Manager / Secret Service; never on disk otherwise |
| SQLite `auth_session` (first-party login only) | user id, access token, refresh token, expiry, refresh-token fingerprint | tokens: yes | tokens are AES-256-GCM ciphertext (versioned, AAD-bound), key from DPAPI / Secret Service / 0600 file; `secure_delete` + WAL truncate on logout |
| `QSettings` | device id, anonymous analytics id, OIDC configuration, window state | no (random ids, config) | none needed |
| consent files, log file, Sentry database | consent decisions; redacted log; crash minidumps | minidumps may contain memory | documented in SECURITY.md |

There is **no cached domain data** (tasks, media metadata, notifications). Since ADR-0034 the database also holds the offline
write queue (`outbox` table, migration 2; payloads are not encrypted, see that ADR). The `kv` session table holds an
already-encrypted session, and in OIDC mode it is not used for tokens at all.

## Decisions

1. **No SQLCipher now.** The only sensitive value in SQLite is encrypted at the column level with a key held in the OS
   credential store, so whole-file encryption would protect nothing additional, would add a dependency (SQLCipher is a
   replacement SQLite build with its own licensing and vcpkg port) and would make every build and packaging path heavier.
   This is a recommendation, not an irreversible step; nothing was changed in the database format (plan rule 7 is not triggered).
   **Revisit when** any feature stores user content offline (task bodies, notes, media metadata, notification text).
   Migration plan for that day, to be confirmed with the owner BEFORE implementation: (a) add SQLCipher as the SQLite provider
   behind `SqliteConnection` (key = the existing master key, `PRAGMA key` before any other statement); (b) bump the schema with a
   migration that is a no-op for SQL but a one-off file conversion step in `DatabaseWorker` (`sqlcipher_export` into a new file,
   verify, swap, keep `aedst.sqlite3.bak` until the next successful start); (c) a database that is not a valid SQLCipher file
   and not a plain SQLite file is renamed aside and recreated, never opened blindly; (d) tests for plain -> encrypted and for
   "interrupted conversion".
2. **All SQLite work runs on one worker thread** (`persistence::DatabaseWorker`). Opening the file, creating the directory,
   applying migrations, probing the OS keyring for the master key (Secret Service IPC on Linux), AES-GCM and every statement happen
   there. Callers get completions through a `Dispatcher` (the GUI thread in the app: a queued `QMetaObject::invokeMethod`).
   Tasks run in submission order; if the database cannot be opened or is newer than this build, every task completes with the
   error and the app keeps running without local storage. Destruction drains the queue and closes the connection on its own thread.
3. **`AsyncTokenStore`** wraps `TokenStore` on that thread. The master key provider and `SecureStorage` are created lazily on the
   worker. `LocalTokenStoreAdapter` serves reads from an in-memory cache (cheap enough for every request), updates the cache only
   after a successful write, and wipes the cache immediately on clear while the disk wipe completes afterwards. At startup the stored
   session is preloaded asynchronously; the login screen shows progress (`restoringSession`) and `AppDependencies` continues exactly like
   the OIDC restore path.
4. **SQLite is not touched at all in OIDC mode**: no worker thread, no file, no keyring probe for the master key.
5. **Schema history is enforced by tests**: the SHA-256 of each shipped migration is frozen (editing a shipped migration fails the
   test; add a new one), every previous version is built by applying only its first N migrations, a row is written, and the
   current build must upgrade it without losing the row; the pragmas (WAL, `foreign_keys=ON`, `busy_timeout=5000`,
   `secure_delete=ON`) and the final column layout are asserted. Migrations stay forward-only, one transaction each, `PRAGMA user_version`.

## Consequences

- The 50 ms `PerformanceBudget` test of ADR-0005 still guards `TokenStore` itself (now on a worker, where it can no longer stall the UI).
- Startup of the first-party path is asynchronous: the first frame is the login view in "restoring" state, then the home screen.
- A storage failure at sign-in is reported to the user; a failure to read the stored session at startup leaves the login screen.
- The first-party path (and with it all of this) goes away at `AEDST_AUTH_FIRST_PARTY_SUNSET`; the worker, migrator and history tests are
  kept because the next offline feature needs them.

## Not verified

Nothing here has been compiled or run. The worker's shutdown ordering (queue drained, connection closed on its thread, completions dropped
when the application object is already gone) and the lazily created keyring provider on a non-GUI thread (libsecret and DPAPI calls are
thread-agnostic, but this has not been exercised under TSan) should be checked by the `tsan` preset on the first CI run.

## Later changes

- **Superseded in part by ADR-0034** (offline outbox): the inventory above said the database holds only the encrypted session.
  Since migration 2 the `outbox` table holds queued write payloads (task titles and descriptions) in clear, and migration 3
  adds the `kv` cache (last capability answer). The "no SQLCipher while only an already-encrypted session is stored" decision
  no longer describes the schema; its stated trigger is the first feature that stores user content offline, which the outbox is.
- **Sign-out clears the media caches**: the disk cache of `image://media` (thumbnails and previews of the user's files), its
  decoded in-memory images and the video thumbnails are removed in `AppDependencies::onSignedOut()` (hook set in `app_run.cpp`;
  `MediaImageFetcher::clear()`, `MediaImageProvider::clear()`, `VideoFrameGrabber::clearThumbnailCache()`), so a second user of
  the machine does not find the first one's pictures.

## Proposal awaiting the owner: SQLCipher (nothing implemented)

**Recommendation: adopt SQLCipher before the outbox carries more than short task text** (attachments metadata, comments), not
before: it adds a dependency (a vcpkg port or a patched SQLite), changes the file format (a reviewed one-way conversion plan for
existing databases is needed) and the key must come from the same store as the AES master key (see the QtKeychain proposal in
ADR-0003). Until then the outbox payload is plaintext at rest, which is stated here so nobody assumes otherwise. The owner
decides when "short task text" stops being enough.
