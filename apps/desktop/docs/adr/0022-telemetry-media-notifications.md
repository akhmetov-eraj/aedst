# ADR-0022: Telemetry, media pipeline, desktop notifications and Qt 6.12

- Status: accepted (not compiled in the authoring environment - no Qt/vcpkg toolchain)
- Scope: `apps/desktop` (Qt/C++ app: `Modules/`, `App/`)
- Supersedes in part: ADR-0006 (notifications: `QSystemTrayIcon` rejected; crash upload transport)
- Superseded in part by: ADR-0027 (native Windows toasts and live delivery; "no push service on desktop" no longer holds once the server streams)

## Context

The shared contracts gained `aedst.notifications.v1`, `aedst.search.v1`,
`aedst.media.v1` and `aedst.admin.v1`. The desktop client needed the matching
features (Tasks as the worked example with attachments, Search, Notifications,
Media, Admin), crash/error reporting that reaches a backend, product analytics
with feature flags, and a dependency refresh.

## Decisions

### 1. Contract

Every RPC of the four new services has an entry in `contract/methods.hpp`
(drift test + `check-proto-freshness.sh`). Reads are `Idempotent`; uploads and
admin mutations are never retried. Push-device RPCs are listed but unused (below).

### 2. Media: Core building blocks, thin feature

`Aedst::Media` (`Modules/Core/Media`, QML module `Aedst.Media`) holds everything
reusable so Tasks can attach files without linking `Features/Media`:

| Need | Implementation (library-first) |
|---|---|
| Upload | `MediaUploadController`: compress -> `CreateUpload` -> `PresignedUploader` (QNetworkAccessManager PUT streaming a `QFile`, `uploadProgress`, abort) -> `CompleteUpload`; queue, cancel (+ best-effort `DeleteMedia` of the orphan), retry (same URL until `expires_at`, else start over) |
| Picking files | QtQuick.Dialogs `FileDialog` + `DropArea` (`MediaUploadPanel.qml`) |
| Compression | `QImageReader` (`setAutoTransform`, `setScaledSize`) + `QImageWriter` (`setQuality`), max dimension 2560, quality 82; EXIF/GPS dropped by re-encoding; animations and non-images pass through |
| Images | `MediaImageProvider` (`QQuickAsyncImageProvider`, `image://media/<id>/<variant>?u=<url>`): decode off-thread, memory `QCache` keyed by media id + variant (presigned URLs rotate), `QNetworkDiskCache` for bytes; only http(s) sources |
| Thumbnails | server variants first (`selectVariant`); fallback `QImage` scaled (local images) and `VideoFrameGrabber` (`QMediaPlayer` + `QVideoSink`, first frame -> PNG in the cache dir) |
| Video | `VideoPlayer.qml`: `MediaPlayer` + `VideoOutput` + `AudioOutput`, keyboard controls |

Qt Multimedia is a Qt add-on module, installed with Qt (`aqt ... -m qtmultimedia`,
CI `modules: qtmultimedia`), not a vcpkg port.

### 3. Notifications on desktop: poll + QSystemTrayIcon

Desktop has no FCM/APNs/Web Push channel. `NotificationsViewModel` polls
`GetUnreadCount` (30 s foreground, 2 min background, exponential backoff to 5 min on
failures, only while signed in), fetches the newest unread items when the count
rises, and `NewNotificationDetector` announces only items created after sign-in, each
once, at most 3 per poll plus one summary, honouring the server preferences
(`push_enabled` = "desktop notifications"). Native display goes through
`platform::NotificationService`: D-Bus on Linux when available, otherwise
**`TrayNotificationService` (`QSystemTrayIcon::showMessage`)** - Windows toast/balloon,
Linux tray - with the in-app `AedstToast` as the fallback.

This reverses ADR-0006's rejection of `QSystemTrayIcon`: it requires `QApplication`
(Qt Widgets) instead of `QGuiApplication`. Accepted cost: Qt6::Widgets is linked and
deployed (a few MB); no Widgets UI is built, the UI stays Qt Quick. WinRT toasts /
`UNUserNotificationCenter` (actions, history) remain the revisit path. A
server-streaming "watch unread" RPC would replace polling without UI changes.

### 4. Crash and error reporting: sentry-native (crashpad)

`SentryReporter` (`Modules/Core/Telemetry`) wraps vcpkg `sentry-native` (default
features: platform backend = crashpad on Windows/Linux, + transport).
`crashpad_handler` is copied next to the executable (`App/CMakeLists.txt`).

- Consent-gated: `require_user_consent`; nothing leaves the machine until the user
  opts in (shared decision with the local `ConsentGatedCrashUploader`).
- `release = "aedst-desktop@<version>"`, environment from the build.
- Identity: the opaque user id only (`sentry_set_user({id})`), cleared on sign-out.
- Breadcrumbs: `ErrorCenter` reports and Qt warnings (redacted with `redactSecrets`).
- When no DSN is configured, or `sentry_init` fails, the existing local
  `CrashHandler` stays in charge (never both: they compete for the same OS hooks).
- Debug symbols: the release CI job uploads `build/<preset>/symbols` with
  `sentry-cli debug-files upload` when `SENTRY_AUTH_TOKEN` (secret) and
  `SENTRY_ORG` / `SENTRY_DESKTOP_PROJECT` (variables) exist.

### 5. Typed error -> UI

`core::ErrorSink` (Qt-free port) is injected into every ViewModel;
`telemetry::ErrorCenter` implements it: logs, adds a breadcrumb, maps the code to a
stable kind (`network`, `unauthenticated`, `permission_denied`, ...), de-duplicates
within 3 s, and emits `errorRaised(kind, message, retryable, blocking)`; `Main.qml`
turns it into a toast (transient) or banner (blocking) with localized titles.
`Unauthenticated` emits `sessionExpired` and signs the user out once.
`core::ErrorCode::PermissionDenied` was added so a forbidden admin call no longer
maps to "unauthenticated" (which would have signed the user out).

### 6. Qt logging bridge

`installQtLogBridge()` installs a `qInstallMessageHandler` that copies Qt/QML
messages at Warning and above into AedstLogger (spdlog, logger `qt`, category kept)
and chains to the previous handler. Both loggers stay (owner decision); Debug/Info
`qt.*` traffic is still not forwarded, so `QT_LOGGING_RULES` cannot flood the log.

### 7. Product analytics and feature flags: PostHog over HTTP

`PostHogAnalyticsTracker` implements the existing `AnalyticsTracker` port with
PostHog's public HTTP API (`POST /batch/`, `POST /flags?v=2`; legacy `/decide`
shape also parsed) through `QNetworkAccessManager` - no SDK. Off unless a project
key is configured **and** the user opted in; anonymous per-install id until
`identify(userId)`; event properties are the small `analytics::events` vocabulary
(never user content such as search text); revoking consent drops queued events.

Why not "no analytics": a template without a consent-gated analytics/flags seam
gets one bolted on per product, usually without consent handling. PostHog is open
source and self-hostable, so the default does not lock products into a vendor, and
the desktop needs no new dependency for it.

### 8. Admin visibility

The Admin entry is shown only when the access token's roles claim
(`AEDST_OIDC_ROLES_CLAIM`, default `roles`, dotted paths supported) contains
`admin` (`network::parseAccessTokenClaims`, signature NOT verified - UI only). The
server's `RoleInterceptor` stays the authority; a `permission_denied` answer shows
an "Access denied" state without signing out.

### 9. Versions (verified 2026-10-07)

- vcpkg `builtin-baseline` `2cfff9c458d9dcf642e9fa09ba624f9931bb5358`
  (`git ls-remote https://github.com/microsoft/vcpkg HEAD`). Port versions at that
  baseline: protobuf 6.33.4, abseil 20260107.1, spdlog 1.17.0, fmt 12.2.0,
  sqlite3 3.53.4, openssl 3.6.5, gtest 1.18.0, sentry-native 0.17.1. (boost-asio was removed in ADR-0024.)
- Qt **6.12.0** (latest in download.qt.io/official_releases/qt/, released
  2026-09-30). Its documented minimums (the macOS 14.4 floor is moot: Desktop no longer targets macOS, ADR-0023):
  Windows 10 1809 (6.12 is the last release supporting Windows 10).
  Whether 6.12 is an LTS release: UNVERIFIED. Flathub's `org.kde.Platform` had no
  6.12 branch yet (latest 6.11) - the Flatpak manifest targets 6.12 and needs that
  runtime (or a temporary 6.11.x pin).
- Flatpak modules re-pinned to the same upstream tags (fmt 12.2.0, spdlog v1.17.0,
  abseil 20260107.1, protobuf v33.4, sentry-native 0.17.1).

## Verification

Nothing here was configured, built or run. Unit tests were added for the pure parts
(contract table, claims parser, link allow-list, snippet parsing/escaping, search
debounce/staleness, notification detection, task validation/mapping, media variant
selection / image-id encoding / compression, PostHog payloads, ErrorCenter). Manual
checks for the first real build: upload a large photo and a video to a task (progress,
cancel, retry after killing the network, thumbnail appears after processing), play the
video, receive a broadcast while minimized (tray toast), force a crash with a DSN set
and consent granted, and confirm the event is symbolicated.

## Revisit when

A streaming unread RPC exists (drop polling); WinRT/UN notifications with actions are
required; Qt adds a Quick-native tray API (drop Qt Widgets); PostHog's `/flags`
response shape changes; Qt drops Windows 10 support and the product floor must stay there (pin the
newest Qt LTS that still supports Windows 10 instead).
