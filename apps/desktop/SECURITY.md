# Security — AEDST Desktop

Status: **statically reviewed, never compiled or run.** Every control below is
implemented in code and covered by tests written for it, but none of those tests
has executed yet. The first real build (see [BUILDING.md](BUILDING.md)) is the
acceptance test; until it is green, treat this document as the design, not as
evidence.

Report vulnerabilities privately to the repository owner (do not open a public
issue).

## Threat model (short)

| Asset | Threat | Control |
|---|---|---|
| Session tokens at rest | Another local user / malware reading the profile | Encrypted with AES-256-GCM; master key in the OS credential store ([ADR-0003](docs/adr/0003-secret-storage.md)) |
| Tokens/passwords in memory | Core dump, swap, crash report | `SecretString` / `secureWipe`; write-only `LoginViewModel.password`; crash reports scrubbed and consent-gated |
| Traffic | MITM, downgrade to http | https required, TLS ≥ 1.2, `ignoreSslErrors` never called, optional SPKI pinning ([ADR-0004](docs/adr/0004-transport-policy.md)) |
| Logs / telemetry | Secrets or PII leaking into support bundles | Redaction layer on every sink (`Aedst::Core`), no bodies/headers logged, analytics is a no-op port by default |
| Updates | Malicious manifest / rollback / tampered installer | Ed25519-signed manifest, key rotation, rollback protection, https-only URLs, SHA-256 + size bound to the signed manifest (checked again after download only where the installer reports the file; for Velopack installs Velopack verifies the package against the feed hash, see the ADR-0029 proposal) ([ADR-0006](docs/adr/0006-platform-services-and-updates.md)) |
| Deep links / second-launch arguments | Injection through `aedst://` URLs or argv | Strict allow-list parser (`parseDeepLink`), length limits, control characters stripped |
| Plugins (code loading) | Unsigned or swapped library loaded into the process | Off by default (`AEDST_ENABLE_RUNTIME_PLUGINS`); when on: only `<app dir>/plugins`, files pinned by an Ed25519-signed allowlist, nothing loads in a release build without it, user-writable directory refused, `qrc:/` routes only, no sandbox ([ADR-0033](docs/adr/0033-feature-plugin-architecture.md)) |
| Memory-safety bugs in the process | Exploitation of a parsing or buffer bug | Exploit mitigations on every first-party target: CFG, `/sdl`, ASLR/high-entropy ASLR/DEP/CET on Windows; stack protector, stack-clash protection, CET, PIE, `_FORTIFY_SOURCE=3`, full RELRO, non-executable stack on Linux (`cmake/AedstHardening.cmake`; the install-verify scripts check the shipped binary) |
| Server responses | Oversized or substituted responses | Unary bodies are aborted past 16 MiB (`ClientConfig::maxResponseBytes`); the OIDC sign-in sends a random `nonce` that the ID token must echo; the session tokens are `SecretString`s and a `Public` request body (login, refresh) is wiped after the call |
| User data left behind | The next user of the machine | Sign-out clears the media disk cache, decoded images and video thumbnails, the outbox and the in-memory view models |
| Release artifacts | Tampered build | Authenticode / detached GPG signatures in CI when secrets exist (unsigned otherwise) |

## Rules for contributors

1. **No secrets in plain files or logs.** Tokens and passwords never go to
   `QSettings`, JSON config or `AedstLogger` (redaction is a safety net, not a
   licence). Persist secrets only through `TokenStore`/`SecureStorage`.
2. **Hold secrets in `aedst::core::SecretString`** or wipe with
   `aedst::core::secureWipe`. `QString` cannot be wiped (implicit sharing) — do
   not store secrets in it beyond the binding hand-off.
3. **Never call `QNetworkReply::ignoreSslErrors` / `QSslSocket::ignoreSslErrors`.**
   Corporate CAs go through `ClientConfig::extraCaCertificatesPem`.
4. **Validate at the boundary.** Deep links: `platform::parseDeepLink`. Update
   manifests: `update::ManifestVerifier`. Server data: Protovalidate on the
   server plus Domain validation here.
5. **Minimum OS permissions.** The Flatpak manifest requests network, display, GPU, audio, the Secret
   Service and the tray watcher (table in `packaging/README.md`); notifications and portals need no permission.
6. **Dependencies:** vcpkg baseline pinned (`vcpkg.json`); Qt pinned
   (`qt-version.txt`). Adding a dependency requires updating
   `docs/content/library-first.md` in the same change.

## Data lifecycle

- **Login:** access/refresh tokens encrypted (`TokenStore::save`), refresh-token
  SHA-256 fingerprint stored for diagnostics.
- **Logout / rejected session:** `TokenStore::clear()` deletes the row, sets
  `secure_delete`, and truncates the WAL so ciphertext does not linger.
- **Crash reports:** written locally to `<data>/crashes`. The ONLY production upload
  path is sentry-native/crashpad, active when a Sentry DSN is configured and the user
  consented (ADR-0022, ADR-0030). Without a DSN nothing is ever uploaded: reports stay
  on the machine. `ConsentGatedCrashUploader` (consent, daily rate limit, size limit,
  path scrubbing) is implemented and unit-tested but has **no production `HttpPoster`**
  and is not wired into the app; it is kept for a possible self-hosted endpoint (ADR-0006).
  Binary minidumps cannot be scrubbed — that is why consent is mandatory.

## Known limitations (honest list)

- `QString`/protobuf copies of the password exist transiently during a login
  request (protobuf `set_password` copies the string) and are not wiped.
- `secure_delete` cannot defeat copy-on-write file systems or SSD wear levelling.
- Certificate pinning guarantees the *response* is discarded on mismatch; on
  Qt's threaded HTTP stack the request bytes may already have been sent.
- Windows: existing sessions from the pre-`MasterKeyProvider` format are not
  readable and users sign in once again.
- OIDC mode (ADR-0025): only the refresh token is persisted, in the OS credential store. With no usable
  credential store (headless Linux without a Secret Service) sign-in fails with an explicit message; there is no file
  fallback for it. The access token is memory-only.
- Linux without a Secret Service (headless, Flatpak without portal): the master
  key falls back to a `0600` file next to the database — weaker, and logged.
- Native toasts carry only a relative in-app route as activation argument (`isActivatableLink`); the route is validated
  again by `Router::openLink`. Text from the server is escaped and length-limited before it reaches the toast XML or D-Bus.
- Signing steps exist in CI but have never run against real
  credentials.
