# ADR-0006: Platform services, crash reporting and update verification

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Platform`, `Aedst::CrashReporting`, `Aedst::Update`, `Aedst::Connectivity`)

## Decisions

**Platform (`Aedst::Platform`)** — each capability is an interface with a
per-OS implementation and a runtime availability check:

- Single instance: `QLockFile` + `QLocalServer` activation channel; a second
  launch forwards sanitized arguments and exits. A live but unreachable lock
  holder yields `Role::Unavailable` (fail open), never lock stealing.
- Deep links: pure allow-list parser `parseDeepLink` (length limits, percent
  decoding validation, control characters rejected).
- Autostart: Windows `HKCU\...\Run`, Linux XDG autostart `.desktop` (atomic
  `QSaveFile`), macOS LaunchAgent plist. Flatpak: portal documented, not
  implemented (`UnsupportedAutostartManager`). `SMAppService` needs Objective-C
  and is documented only.
- Notifications: `QtDBus` (`org.freedesktop.Notifications`) on Linux when
  available, otherwise an in-app signal for `AedstToast`. Native Windows toast
  (WinRT) and macOS `UserNotifications` are **not implemented**; `QSystemTrayIcon`
  was rejected because it forces `QApplication`/Qt Widgets on a Qt Quick app.

**Crash reporting (`Aedst::CrashReporting`)** — local capture only (Windows
`MiniDumpWriteDump`; POSIX `sigaction` + `sigaltstack` + `backtrace_symbols_fd`).
Upload happens only through `ConsentGatedCrashUploader` (consent, daily rate
limit, size limit, path scrubbing) over an injected `HttpPoster`. The
signal/minidump path is process-fatal code and is verified manually (checklist in
its `CMakeLists.txt`), not by unit tests.

**Updates (`Aedst::Update`)** — verification only, no auto-install. The manifest
is an envelope (`base64 payload` + `signatures[{keyId, signature}]`); the signed
bytes are the exact decoded payload. Ed25519 via OpenSSL EVP; trusted keys
embedded at build time through `AEDST_UPDATE_PUBLIC_KEYS` (`keyId:hex64,...`,
supports rotation; empty = reject everything). Rollback protection (version and
`publishedAt`, persisted through `UpdateStateStore`), https-only URLs, streaming
SHA-256 with constant-time compare and a size cap. Release CI must supply the
public keys; the app layer must implement `HttpFetcher` (https, certificate
validation, size cap) before the checker is started.

**Connectivity (`Aedst::Connectivity`)** — `QNetworkInformation`; only
`Disconnected` maps to Offline (LAN/localhost servers keep working when the OS
reports `Local` or `Unknown`).

## Revisit when

Windows/macOS native notifications are required (add an Objective-C++/WinRT
backend behind `NotificationService`), or an auto-updater is wanted (then add a
download+install stage after `ArtifactVerifier`, with OS-level signature checks).
