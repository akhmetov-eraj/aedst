# ADR-0003: Secret storage — master key in the OS credential store, AES-256-GCM envelope

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Persistence`)
- Related: [SECURITY.md](../../SECURITY.md), [library-first.md](../../../../docs/content/library-first.md)

## Context

Sessions must survive restarts, so tokens are persisted. The previous design
encrypted with DPAPI on Windows but on every other OS kept an AES key in a file
next to the database — no better than storing the token itself, and the file was
created and `chmod`-ed in two steps (a window governed by `umask`). The standard
requires the system secret store on each OS.

## Decision

`SecureStorage` always encrypts with AES-256-GCM (OpenSSL EVP), blob layout
`[version=1][12-byte IV][ciphertext][16-byte tag]`, AAD `aedst.secure.v1`;
unknown versions are rejected. The **master key** comes from a `MasterKeyProvider`
chosen at startup by `createPlatformMasterKeyProvider()`:

| OS | Provider | Storage |
|---|---|---|
| Windows | `DpapiMasterKeyProvider` | random 32-byte key wrapped by `CryptProtectData` (fixed optional entropy), file `secure.key.dpapi` |
| Linux | `KeyringMasterKeyProvider` + libsecret backend | Secret Service via synchronous `libsecret` calls (optional, `AEDST_ENABLE_LIBSECRET`) |
| fallback | `FileMasterKeyProvider` | key file created atomically with `open(O_CREAT|O_EXCL, 0600)` — used when no keyring is reachable (headless, Flatpak without portal) |

The selected provider is logged (never the key). A legacy `secure.key` is
migrated into an available keyring, then overwritten and removed. The key is
cached once in a wiped-on-destroy buffer.

## Consequences

- Platform APIs only (DPAPI, Security.framework, libsecret) — no new vcpkg
  dependency; libsecret is an optional system package (`libsecret-1-dev`).
- Windows sessions created before this change cannot be decrypted; users sign in
  once again. Acceptable pre-release.
- The file fallback is weaker (key beside data) and is a documented, logged
  degradation, not a silent default.

## Revisit when

QtKeychain (or a Qt-native secret API) becomes an approved dependency and covers
all three platforms without a synchronous-API mismatch; or the threat model
requires hardware-backed keys (TPM / Secure Enclave).
