# aedst-android

The AEDST Android client: a multi-module app mirroring the module boundaries
of `apps/server` (see [../../docs/content/architecture-android.md](../../docs/content/architecture-android.md),
[../../docs/content/architecture.md](../../docs/content/architecture.md) and
[../../docs/content/modules.md](../../docs/content/modules.md) — or the published docs site).

> **Verification status.** The previous baseline of this template (before the
> session/offline/security work described below) was verified on Windows 11 with
> JDK 21, Android SDK 37 and Gradle 9.7.1: `assembleDebug`, `assembleRelease`
> (R8), `:benchmark:assemble`, `testDebugUnitTest`, `lintDebug`, `detekt` and
> `ktlintCheck`. **The changes since then (token refresh, outbox/WorkManager,
> network security config, localized errors, new tests) were written without a
> JDK/Android SDK available and have NOT been compiled or run yet.** Run
> `make generate && ./gradlew ktlintCheck detekt lintDebug testDebugUnitTest assembleDebug`
> first; expect to fix small compile-level issues. **Also never compiled:** core/media,
> feature/tasks|search|notifications|media|admin, Sentry/PostHog telemetry, FCM push and the
> dependency-update plugins (added 2026-10 without a JDK/SDK). Also never run: the app on a
> device, the Keystore/Tink code paths, Firebase at runtime with a real
> `google-services.json`, baseline-profile generation, and the `androidTest`
> (Accessibility Test Framework) suites.

## Module map

```
app/                        application module — Hilt entry point, NavHost, wires every feature

core/common                 Outcome/AedstError, DispatcherProvider, UseCase base classes
core/ui                     Compose theme + reusable components
core/network                Generated ConnectRPC/Protobuf client (connect-kotlin) over OkHttp: auth interceptor, 401 refresh authenticator, TLS pinning, NetworkMonitor, error mapping — run `make generate` first (see buf.gen.yaml)
core/database                Room (AppDatabase, BaseDao, outbox table) — unencrypted: non-sensitive data only
core/sync                    Offline write queue: OutboxRepository + WorkManager OutboxSyncWorker + OutboxHandler contract
core/datastore               Proto DataStore
core/security                Android Keystore + Tink AEAD encrypted store (DataStore) + Play Integrity facade
core/analytics               Telemetry: Sentry (crashes, errors, OkHttp spans + traceparent, nav breadcrumbs) + PostHog (consent-gated analytics, feature flags)
core/navigation               AedstNavGraph contract + AedstRoutes (+ routeForLink for server in-app links) — the feature/api boundary
core/media                    MediaService repository, upload pipeline (Photo Picker → WorkManager: compress + CreateUpload → presigned PUT → CompleteUpload), Coil images, Media3 player/Transformer, local thumbnails
core/testing                  MockK/Turbine/coroutine test rules shared across feature tests

feature/auth                 complete worked example: domain/data/presentation/navigation (+ analytics consent toggle)
feature/tasks                tasks list / detail / create; attachments on the detail screen (core/media)
feature/search               debounced search-as-you-type, U+E000/U+E001 snippet highlighting
feature/notifications        inbox, unread badge, mark read, preferences; FCM push (channels, token sync, deep links)
feature/media                media library + image/video viewer
feature/admin                overview/users/tasks/audit — only shown for the `admin` role from the access token

benchmark/                    Macrobenchmark module: app-startup benchmark skeleton
build-logic/                  convention plugins (composite build, not buildSrc)
```

Dependency direction: `feature/*` -> `core/*`, never the reverse; never
`feature/*` -> `feature/*` except through `core/navigation`'s route
contracts. See [../../docs/content/architecture-android.md](../../docs/content/architecture-android.md)
for the full rule and how it maps to the server's `api/` boundary.

## Build locally

Requires the Android SDK (compileSdk 37) and JDK 21 (detekt 1.23 cannot run on
JDK 25). Generate the ConnectRPC stubs first (needs the `buf` CLI), then:

```bash
make generate                                                  # buf codegen into core/network/src/main/kotlin-gen
cp app/google-services.json.example app/google-services.json   # placeholder is enough to compile
./gradlew detekt ktlintCheck testDebugUnitTest
```

| Command | What it does |
|---|---|
| `./gradlew detekt` | static analysis |
| `./gradlew ktlintCheck` / `ktlintFormat` | formatting check / autofix |
| `./gradlew testDebugUnitTest` | JVM tests (JUnit, MockK, Turbine, MockWebServer, Robolectric for Compose/Room/Worker) |
| `./gradlew connectedDebugAndroidTest` | Compose UI tests with Accessibility Test Framework checks — needs an emulator/device |
| `./gradlew :app:assembleRelease -Paedst.certPins=sha256/<live>,sha256/<backup>` | release build; refuses to assemble without pins (`-Paedst.allowUnpinnedRelease=true` to opt out) |
| `./gradlew :app:assembleDebug` | build the debug APK |
| `./gradlew :benchmark:connectedCheck` | Macrobenchmark — needs a connected device/emulator |
| `./gradlew :app:generateBaselineProfile` | regenerate the app's baseline profile — needs a device/emulator |

## Dependency and platform decisions

- **Versions policy**: latest *stable* release of every dependency, verified
  against Maven Central / Google Maven metadata (never alpha/beta/rc). Compose
  versions come from the BOM (`platform(libs.compose.bom)` in the compose
  convention plugin), not from individual pins.
- **AGP 9 with built-in Kotlin + Hilt 2.60.1** (DI decision). Weighed:
  (a) AGP 9 + latest Hilt versus (b) migrating to Metro or Koin. Chose (a). The
  AGP 9 migration is small (drop the `org.jetbrains.kotlin.android` plugin from
  the convention plugins and `benchmark`; KSP, Hilt, Compose and serialization
  plugins work unchanged). Hilt keeps compile-time graph validation and the
  first-party integrations this app uses (`hilt-navigation-compose`,
  `hilt-work`, `@HiltViewModel`) stay supported by Google. Metro is promising
  and KMP-ready, but this is an Android-only client, so KMP-readiness has no
  payoff today, and moving would mean replacing the Hilt navigation/work
  integrations and rewriting every module for no functional gain; Koin gives up
  compile-time safety. Revisit Metro only if a KMP shared module is introduced.
- **Secrets storage**: `androidx.security:security-crypto`
  (`EncryptedSharedPreferences`) is deprecated and removed. `core/security` now
  uses Android Keystore (non-exportable master key) + Google Tink AEAD
  (`AndroidKeysetManager`, AES-256-GCM) sealing a DataStore file
  (`AeadSerializer`). Data written by the old implementation is migrated once
  (`LegacyEncryptedPreferencesMigration`: read old, write new, then delete old);
  delete that class once all supported installs have upgraded.
- **connect-kotlin 0.9.0** (latest, 2026-06-12) on **OkHttp 5.5.0**
  (`connect-kotlin-okhttp` 0.9.0 links OkHttp 5.4.x; same 5.x API). OkHttp 5.5
  requires compileSdk 37. The repo is active (commits within days, a release
  every 1-3 months, 9 open issues) but is a small pre-1.0 project. **Plan B** if
  it stalls or breaks: apps/server (connect-go) also serves the gRPC and gRPC-Web
  protocols on the same handlers, so the client can move to **Wire** (Kotlin-first,
  KMP-capable, generates gRPC clients over OkHttp) or **gRPC-Kotlin**
  (`grpc-okhttp`) with no server change. Only `data/repository` classes touch the
  generated stubs, so the swap stays local to `core/network` and each feature's
  `data` package.

## Telemetry, push and media (configuration)

Nothing below is required to build or run the template; an empty value disables that integration.

| What | Where | Effect when missing |
|---|---|---|
| `app/google-services.json` (Firebase project, FCM only) | gitignored; `google-services.json.example` is a placeholder | google-services plugin not applied, no FirebaseApp, push registration skipped |
| `-Paedst.sentryDsn` / `SENTRY_DSN` | Gradle property or env (core/analytics) | Sentry not initialised |
| `SENTRY_AUTH_TOKEN`, `SENTRY_ORG`, `SENTRY_PROJECT` | CI env (Sentry Gradle plugin) | R8 mapping not uploaded (UUID still embedded) |
| `-Paedst.posthogKey` / `POSTHOG_KEY`, `-Paedst.posthogHost` | Gradle property or env | PostHog not initialised; flags return defaults |
| `-Paedst.environment` | Gradle property or env | `development` (debug) / `production` (release) |

- **Errors**: Connect codes map to typed `AedstError` kinds (`core/network/ApiError.kt`), shown as localized
  snackbars via `AedstErrorSnackbarEffect` (`core/ui`). Uncaught exceptions: Sentry's handler; application-scope
  coroutine failures: `ErrorReporter` (non-fatal).
- **Tracing**: the API OkHttp client ends with Sentry's interceptor (spans, breadcrumbs, W3C `traceparent` to the
  API host only); without Sentry, `RequestHeadersInterceptor` still sends a random `traceparent`.
- **Analytics** is opt-in (Account screen toggle, stored in `UserPrefs.analytics_consent`), like web's consent cookie.
- **Push**: FCM `data` keys expected from the server: `kind`, `title`, `body`, `link` (relative route such as
  `/tasks/<id>`), `notification_id`. Links are validated by `AedstRoutes.routeForLink` before navigating.
- **Media**: Photo Picker (no storage permission) → `MediaUploadScheduler` → WorkManager chain
  `PrepareUploadWorker` (ImageDecoder/WEBP or JPEG, Media3 Transformer H.264 720p, SHA-256, `CreateUpload`) →
  `TransferUploadWorker` (presigned PUT with progress, foreground `dataSync`) → `CompleteUploadWorker`. A retry
  restarts only the failed step.

## Dependency updates

`./gradlew dependencyUpdates --no-configuration-cache` (report, ben-manes) and `./gradlew versionCatalogUpdate`
(rewrites `gradle/libs.versions.toml`, keeps comments and section order). Both reject alpha/beta/rc/cr/m/preview/
eap/dev/snapshot versions; `detekt` is pinned (see decisions). Gradle: `./gradlew wrapper --gradle-version latest`.

## Deliberate scope cuts (see the task's own notes for the full list)

- **Metalava / binary-compatibility-validator**: not wired. It matters most
  for a published library API surface; this app has none yet. TODO once
  `core/*` modules are consumed outside this repo.
- **SonarCloud**: no live project (needs an external account/token). A
  `sonar-project.properties` shape to add when one exists:
  ```properties
  sonar.projectKey=aedst_android
  sonar.organization=<org>
  sonar.sources=app/src/main,core,feature
  sonar.tests=app/src/test,core,feature
  sonar.java.binaries=**/build/intermediates/javac/**/classes
  sonar.kotlin.source.version=2.4
  ```
- **Fastlane**: not installed (needs Play Console / App Store Connect
  credentials). CI stops at `assembleDebug`; a real pipeline would add a
  `fastlane/Fastfile` with `internal` -> `beta` -> `production` Play tracks.
- **architecturetest equivalent**: `scripts/check-feature-boundaries.sh`,
  wired into `android-ci.yml`'s `lint` job — see
  docs/content/architecture-android.md's "Boundary enforcement" section.
