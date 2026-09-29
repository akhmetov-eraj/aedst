# aedst-android

The AEDST Android client: a multi-module app mirroring the module boundaries
of `apps/server` (see [../../docs/content/architecture-android.md](../../docs/content/architecture-android.md),
[../../docs/content/architecture.md](../../docs/content/architecture.md) and
[../../docs/content/modules.md](../../docs/content/modules.md) — or the published docs site).

> **This environment has no Android SDK installed.** Nothing here was built,
> linted, or run — the Gradle files and Kotlin sources were written by hand
> to be internally consistent (module graph, convention-plugin ids, version
> catalog aliases, package names). Open the project in Android Studio and
> run a sync before trusting it further; fix anything the IDE flags on first
> sync, especially the versions marked `# verify on first sync` in
> `gradle/libs.versions.toml`.

## Module map

```
app/                        application module — Hilt entry point, NavHost, wires every feature

core/common                 Outcome/AedstError, DispatcherProvider, UseCase base classes
core/ui                     Compose theme + reusable components
core/network                Generated ConnectRPC/Protobuf client (connect-kotlin) over OkHttp, auth interceptor, error mapping — run `make generate` first (see buf.gen.yaml)
core/database                Room (AppDatabase, BaseDao)
core/datastore               Proto DataStore
core/security                Keystore/EncryptedSharedPreferences + Play Integrity facade
core/analytics               Crashlytics/Performance/Analytics facade + no-op impl
core/navigation               AedstNavGraph contract + AedstRoutes — the feature/api boundary
core/testing                  MockK/Turbine/coroutine test rules shared across feature tests

feature/auth                 complete worked example: domain/data/presentation/navigation

benchmark/                    Macrobenchmark module: app-startup benchmark skeleton
build-logic/                  convention plugins (composite build, not buildSrc)
```

Dependency direction: `feature/*` -> `core/*`, never the reverse; never
`feature/*` -> `feature/*` except through `core/navigation`'s route
contracts. See [../../docs/content/architecture-android.md](../../docs/content/architecture-android.md)
for the full rule and how it maps to the server's `api/` boundary.

## Build locally

Requires Android Studio "Quail" (or the matching command-line SDK/AGP) and a
JDK 21. Not verified in this environment — after opening the project:

```bash
cp app/google-services.json.example app/google-services.json   # placeholder is enough to compile
./gradlew detekt ktlintCheck testDebugUnitTest
```

| Command | What it does |
|---|---|
| `./gradlew detekt` | static analysis |
| `./gradlew ktlintCheck` / `ktlintFormat` | formatting check / autofix |
| `./gradlew testDebugUnitTest` | JVM unit tests (JUnit + MockK + Turbine) |
| `./gradlew :app:assembleDebug` | build the debug APK |
| `./gradlew :benchmark:connectedCheck` | Macrobenchmark — needs a connected device/emulator |
| `./gradlew :app:generateBaselineProfile` | regenerate the app's baseline profile — needs a device/emulator |

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
