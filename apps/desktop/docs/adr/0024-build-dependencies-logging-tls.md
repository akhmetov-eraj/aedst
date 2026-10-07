# ADR-0024: Fewer dependencies — no Asio, one logging front door per layer, explicit TLS backend

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Network`, `Aedst::Logging`, `Aedst::Telemetry`, `Aedst::Persistence`, `App/`)
- Supersedes in part: ADR-0002 (the `asio` vcpkg feature), ADR-0022 (the "both loggers stay, Qt is not forwarded" rule)

## Context

Phase 3 of the desktop plan asked to (1) drop Boost.Asio, (2) unify logging on Qt logging
categories with a custom message handler and drop spdlog/fmt unless still needed, and
(3) use Schannel on Windows and the system OpenSSL on Linux, shipping no OpenSSL of our own
unless a dependency requires it.

## Decisions

### 1. Boost.Asio removed

The only Asio code was `asio_support.hpp`, an alias for `io_context` that nothing included.
Removed together with the `AEDST_ENABLE_ASIO` option, the `asio` vcpkg feature, the
`Boost::headers` link and the Flatpak flag. Qt Network is the only transport. No real
reason to keep Asio was found.

### 2. Logging: spdlog stays the sink, Qt categories become the front door for Qt code

spdlog/fmt were **not** removed (owner decision, confirmed in Phase 3):

- `AedstLogger` serves Qt-free layers (Domain/Data, Persistence, Update, CrashReporting) and
  GoogleTest binaries that run without a `QCoreApplication`; replacing it would force Qt
  into those layers. It already provides the rotating file, JSON Lines output, level
  filter, correlation ids and secret redaction.
- `fmt` is also used directly by `crash_handler.cpp`.

What changed:

- New leaf module `Aedst::Logging` (Qt Core only) declares `QLoggingCategory`s:
  `aedst.app`, `aedst.network`, `aedst.platform`, `aedst.media`, `aedst.navigation`,
  `aedst.telemetry`. Qt-aware code logs with `qCDebug/qCInfo/qCWarning/qCCritical`.
- The existing Qt message handler (`installQtLogBridge`, `Aedst::Telemetry`) forwards
  `aedst.*` categories at **every** level into `AedstLogger` (spdlog's configured level
  decides what is written) and framework messages (`qt.*`, QML, default category) from
  Warning up, as before. Redaction applies to both. Breadcrumbs for Sentry are unchanged.
- Runtime control: `QT_LOGGING_RULES`, or `AEDST_LOG_RULES` (same syntax, `;` separated,
  applied last); `AEDST_LOG_FORMAT=json` switches the file to JSON Lines.
- Migrated: `ConnectClient`, `PresignedUploader`, `MediaUploadController`, `Router`,
  `SingleInstanceGuard`, `main.cpp`, `AppDependencies`. Not migrated on purpose: Persistence,
  Update, CrashReporting, Analytics, Auth Data, and `Aedst::Telemetry`'s own classes (Qt-free,
  or the owners of the bridge — logging through the bridge from inside it would only add
  re-entrancy).
- Message text is `printf`-style (Qt's category macros), not `{}`-style.

### 3. TLS backend and OpenSSL

- `selectTlsBackend()` (`main.cpp`) activates Qt's **Schannel** backend on Windows and
  **OpenSSL** on Linux before any network access, and logs the active backend. Qt's official
  Linux binaries load the system `libssl` at run time; inside Flatpak it comes from the KDE
  runtime. Nothing in our install tree ships OpenSSL for Qt.
- The vcpkg `openssl` port **stays**, but only as a crypto library: AES-256-GCM
  (`SecureStorage`), Ed25519 and SHA-256 (`Aedst::Update`), `RAND_bytes`. Qt has no AEAD or
  Ed25519 API, and writing those with a platform API (BCrypt/CNG vs. OpenSSL) would be a
  second implementation per OS, which the Library-First order rejects. It is linked
  statically through the vcpkg triplets (`x64-windows-static-md`, `x64-linux`), so no
  `libssl`/`libcrypto` DLLs/SOs are installed by our packages.
- `Aedst::Persistence` no longer links `OpenSSL::SSL`; it only needs `OpenSSL::Crypto`.
- **UNVERIFIED:** whether `sentry-native` (crashpad, Linux transport via libcurl) or any other
  vcpkg port pulls in OpenSSL on its own. Check with `vcpkg depend-info sentry-native` once a
  toolchain is available and record the answer here; it does not change the decision above
  because our own code needs OpenSSL anyway.

## Consequences

- A release on Linux needs a system OpenSSL 3 (or the Flatpak runtime's); without it
  `QSslSocket::supportsSsl()` is false and `ConnectClient` refuses https calls.
- Corporate TLS inspection roots on Windows are honoured via Schannel's OS trust store.
- Two logging front doors exist by design; the rule is "Qt-aware → categories, Qt-free →
  `AedstLogger`", stated in `logger.hpp` and `log_categories.hpp`.
- New tests in `Modules/Core/Telemetry/tests/telemetry_test.cpp` cover the bridge (category
  forwarding, framework-info filtering, redaction). They have not been compiled.

## Revisit when

A Qt-free layer no longer needs logging (then collapse to categories only), a toolchain
shows `vcpkg depend-info` removes the last OpenSSL consumer other than our crypto, or Qt
gains AEAD/Ed25519 primitives.
