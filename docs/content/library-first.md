# BOLIPLARE — Library-First Development Standard

## Главный принцип

BOLIPLARE использует **Library-First Development**. Перед написанием любого нового кода AI-агент или инженер обязан проверить порядок источников решения, а не начинать с "напишу сам":

```
STANDARD LIBRARY
      ↓
PLATFORM API
      ↓
EXISTING BOLIPLARE DEPENDENCY
      ↓
APPROVED LIBRARY
      ↓
CUSTOM CODE   ← только если ничего выше не подошло
```

**Запрещено** самостоятельно реализовывать то, что уже надёжно предоставляется существующей библиотекой или платформенным API — HTTP client, form validation, date formatting, drag & drop, JWT, env-parser, job queue, WebSocket, protobuf validation, state management, сериализация, крипто, UI-компоненты. Полный список анти-паттернов — ниже.

> **Для AI-агентов (включая Claude в этой и будущих сессиях): это обязательное архитектурное правило, а не рекомендация.** Реализация, нарушающая Library-First без задокументированной технической причины, считается не соответствующей архитектуре BOLIPLARE — даже если код технически работает.

> Принятые решения, обоснования и условия пересмотра — [decisions.md](/decisions). Отвергнутые предложения — раздел «Explicit rejections» ниже.

## Обязательный workflow

```
TASK → UNDERSTAND REQUIREMENT → CHECK EXISTING CODE
     → CHECK EXISTING BOLIPLARE LIBRARIES → CHECK PLATFORM STANDARD API
     → CHECK OFFICIAL / RECOMMENDED LIBRARY → CHECK EXISTING INTERNAL UTILITY
     → REUSE
     → (только если ничего не подошло) → IMPLEMENT CUSTOM CODE
```

Перед созданием utility/helper/hook/component/service/repository/parser/formatter/validator/serializer/cache/queue/retry/HTTP-client/WebSocket-layer/auth-mechanism/crypto/date-time/file-handling/image-processing/drag-and-drop/state-management/animation/testing-utility — обязательно пройти Dependency Discovery: стандартная библиотека → BOLIPLARE stack → установленные зависимости → официальный API платформы → рекомендуемая ecosystem-библиотека → существующая реализация в проекте.

## Version policy

**Latest stable, проверенная по реестру. Версии никогда не выдумываются.**

- Перед добавлением или повышением версии любой зависимости (npm, Go module, Maven/Gradle, SwiftPM, vcpkg) — свериться с официальным реестром (npm registry, `go list -m -versions`, Maven Central/Google Maven, GitHub releases/tags, vcpkg baseline) и взять **последнюю стабильную** версию. Pre-release/RC/beta — только с задокументированной причиной.
- AI-агент **не имеет права** писать в манифест номер версии «по памяти» или «примерно». Если реестр недоступен — оставить задачу с пометкой `UNVERIFIED` и сказать об этом явно, а не подставлять правдоподобное число.
- Версии тулчейнов пинятся в одном месте (см. «Monorepo tooling» → `mise.toml`; для Desktop — `apps/desktop/qt-version.txt`). Дублировать версию в нескольких файлах запрещено.
- Потолки версий допустимы только с комментарием и записью в [decisions.md](/decisions) (пример: Apple — `swift-perception` `< 2.0.12`, чтобы сохранить iOS 13; Android — JDK 21, потому что detekt 1.23 не работает на JDK 25).
- Если что-то не удалось проверить (например, Apple/Desktop не компилировались в этом окружении) — документ обязан это прямо сказать; «выглядит правильно» не является верификацией.

## Approved before first use — машинно проверяется

Политика «библиотека утверждена ещё до первого использования» **сохраняется**, но теперь это не соглашение, а проверка в CI:

| Платформа | Механизм | Где живёт allowlist |
|---|---|---|
| Web | **knip** (`apps/web/knip.jsonc`) — падает на неиспользуемых зависимостях/экспортах | `ignoreDependencies` в `knip.jsonc` = утверждённые, но ещё не подключённые библиотеки |
| Server | `go mod tidy` (diff в CI) + **golangci-lint `depguard`** | список разрешённых/запрещённых импортов в `apps/server/.golangci.yml`; `architecturetest` |
| Android / Apple / Desktop | ревью + Version Catalog / `Tuist/Package.swift` / `vcpkg.json` как единственные манифесты | — |

Правила:

1. **Добавление записи в allowlist (knip `ignoreDependencies`, depguard allow-list, `Tuist/Package.swift`, `vcpkg.json`, `libs.versions.toml`) требует обновления этой страницы** (таблица платформы + при необходимости запись в [decisions.md](/decisions)) в том же PR. Allowlist без записи в docs — нарушение.
2. Позиция в allowlist — не вечная: если библиотека не подключена в течение одного релизного цикла, её удаляют из allowlist и из манифеста.
3. Test/dev-only пакеты (Vitest, Testing Library, Playwright, knip, Biome, типы и т.п.) — только в `devDependencies` (Web) / test-scope (остальные платформы).
4. Один primary-инструмент на ответственность (см. ниже) — allowlist не место для второго конкурента.

## Approved stack по платформам

### Web

Next.js (App Router) + React 19. Biome — единственные линтер и форматтер (ESLint и Prettier **не используются** и не добавляются). Изоляция фич — `apps/web/scripts/check-feature-boundaries.sh`. Окружение — Zod-схемы в `src/shared/env/{client,server}.ts` (не читать `process.env` напрямую в коде приложения).

| Ответственность | Инструмент |
|---|---|
| Forms | React Hook Form |
| Validation | Zod (`zod/mini` in code that ships to the browser; full `zod` on the server) |
| Dates | date-fns |
| Drag & Drop | dnd-kit |
| UI | shadcn/ui (`apps/web/src/shared/ui/`); `cn` — npm-пакет из shadcn-экосистемы |
| Animations | Motion |
| URL State | nuqs |
| React utilities (debounce, media query, local storage и т.п.) | React Use |
| Server State | TanStack Query |
| Tables | TanStack Table |
| Virtualization | TanStack Virtual |
| Testing | Vitest + Testing Library + MSW / Playwright — проекты `chromium`, `firefox`, `webkit`, `mobile-chrome`, `mobile-safari` (`@axe-core/playwright` for a11y); контрактный тест mock-API против proto-дескрипторов (`pnpm test:contract`, опционально против реального сервера через `AEDST_CONTRACT_SERVER_URL`) |
| Performance budgets | `scripts/check-bundle-size.mjs` (node built-ins) + Lighthouse CI (`@lhci/cli`, run via `pnpm dlx`, not a dependency) |
| Lint/format | Biome (только он) |
| Unused deps/exports | knip |
| API | сгенерированный Protobuf + ConnectRPC клиент (Connect-ES) |
| Streaming / отмена / композиция async | TanStack Query, `AbortController`, async iterators, Connect server streaming — **не RxJS** |
| AI/LLM интеграция (стриминг чата, tool calls, structured output) | Vercel AI SDK (`ai`) |
| Error tracking / crash reporting | Sentry (`@sentry/nextjs`) |
| Product-аналитика / feature flags | PostHog (`posthog-js`) — только после согласия, без DNT/GPC, через rewrite `/ingest`; `track()` и `useFeatureFlag` в `shared/lib/analytics` |
| Client UI state | Zustand — **только** UI-состояние (настройки, очередь загрузок, палитра); серверные данные — только TanStack Query |
| PWA / Service Worker | Serwist (`serwist` + `@serwist/turbopack`): offline shell, push, `notificationclick`; HTML и ответы API не кэшируются |
| Web Push | стандартный Push API + VAPID (без Firebase SDK; Safari доставляет через APNs) |
| Медиа | платформенные API: `createImageBitmap` + `OffscreenCanvas` (сжатие), XHR (прогресс загрузки по presigned URL), нативный `<video>` |
| i18n | next-intl |
| Графики | Recharts |
| Command palette | cmdk |
| Карусель | Embla Carousel |
| Date picker | react-day-picker |
| OTP-инпут | input-otp |
| Resizable-панели | react-resizable-panels |
| Тосты/уведомления | Sonner |
| Переключение темы (light/dark) | next-themes |

**Примечание.** Vercel AI SDK, React Use, Sentry, PostHog, Zustand, Serwist, next-intl и shadcn/ui-экосистемные библиотеки (Recharts, cmdk, Embla, react-day-picker, input-otp, react-resizable-panels, Sonner, next-themes) утверждены **до первого использования** — это ровно те записи, что лежат в allowlist `knip.jsonc`. См. «Approved before first use» выше: добавление в allowlist = обновление этой таблицы.

**RxJS удалён и не утверждён.** Возвращать его можно только при конкретной задаче, которую не решают TanStack Query / `AbortController` / async iterators / Connect server streaming; решение фиксируется в [decisions.md](/decisions).

**CSP и security headers.** Страничный CSP строится на каждый запрос в `apps/web/src/proxy.ts` (`src/app/config/security/csp.ts`): nonce + `strict-dynamic`, без `unsafe-inline` для скриптов, `connect-src 'self'` (браузер ходит только на свой `/api/rpc`). Статические заголовки — `next.config.ts`. Любой новый сторонний origin (аналитика, шрифты, iframe, CDN) **обязан** быть добавлен в CSP в том же PR и, если он что-то измеряет, — за согласием (`ConsentLayer`).

**Сессия.** Токены только в httpOnly-cookie (BFF: Server Actions + `proxy.ts` + `/api/rpc`); `localStorage`/`sessionStorage` для токенов запрещены.

### Server (Go)

| Ответственность | Инструмент |
|---|---|
| Router | Chi (middleware-экосистема). **Никогда `chimw.RealIP`** — использовать `ClientIPFromXFFTrustedProxies` |
| API Contract | Protobuf |
| API Generation | Buf |
| RPC | ConnectRPC |
| Validation | Protovalidate — реально применяется через `connectrpc.com/validate` |
| Database | PostgreSQL |
| Driver | pgx |
| SQL | sqlc (`sqlc vet` в CI) |
| Migrations | Goose — по одной таблице версий на модуль (`goose_<module>`); линт миграций — squawk (`scripts/lint-migrations.sh`, `task lint:migrations`) |
| Jobs | River — **только отложенные задачи** (deferred jobs), не события |
| Events | Postgres transactional outbox + Relay + in-process bus (`internal/platform/{outbox,messaging,inbox}`); подписчики защищены inbox'ом. `messaging.Publisher` — шов для будущего брокера |
| Cache | Valkey (клиент go-redis, env `AEDST_VALKEY_*`) — только кэш и rate limiting, **никогда не источник корректности** |
| Distributed Locks | Postgres advisory locks (`internal/platform/lock`) |
| Rate Limiting | redis_rate (поверх Valkey) |
| JWT / JOSE | jwx v4 (+ `jwkfetch`); только asymmetric-алгоритмы из allow-list |
| Identity | Сервер — OIDC resource server (`AEDST_OIDC_*`), см. [ADR-0001](https://github.com/aedst/aedst/blob/main/apps/server/docs/adr/0001-identity-provider.md) |
| Configuration | caarlos0/env |
| Observability | OpenTelemetry (OTLP через стандартные `OTEL_*`), otelhttp + otelconnect; любой OTLP-бэкенд |
| Health | `/livez`, `/readyz` (Valkey — optional-проверка: `degraded`, не 503), стандартный gRPC health (`connectrpc.com/grpchealth`, тот же `Readiness`), graceful shutdown |
| gRPC reflection | `connectrpc.com/grpcreflect` — только при `AEDST_GRPC_REFLECTION_ENABLED=true` (по умолчанию выключено: раскрывает всю поверхность API) |
| Retention | `internal/platform/retention` — River periodic jobs (leader-elected), одна `Policy` на append-only таблицу |
| Integration Testing | Testcontainers + `internal/platform/postgrestest` (или `AEDST_TEST_DATABASE_URL`) |
| Object storage | Порт `internal/platform/storage.ObjectStorage` (`Put/Get/Delete/Head/CreateUploadURL/CreateDownloadURL`); одна реализация на **aws-sdk-go-v2** (`service/s3` + presign) с пресетами `AEDST_STORAGE_PROVIDER=s3\|minio\|r2`; `Memory` для тестов. SDK импортируется только из `platform/storage` (depguard) |
| Push | `internal/platform/push`: Web Push (RFC 8291/8292 на stdlib `crypto/ecdh`+`hkdf`, VAPID через jwx v4), FCM HTTP v1 (`golang.org/x/oauth2/jwt`), APNs (HTTP/2, provider token через jwx v4). **webpush-go отклонён** — тянет вторую JOSE-библиотеку |
| Обработка медиа | River-очередь `media`; изображения — `golang.org/x/image/draw` (thumbnail/preview/optimized, JPEG/PNG); видео/аудио — внешний `ffmpeg`/`ffprobe` (`AEDST_MEDIA_FFMPEG_PATH`), опционально |
| HTTP hardening | `http.Server` с `ReadHeaderTimeout/ReadTimeout/WriteTimeout/IdleTimeout/MaxHeaderBytes` из env, TLS ≥ 1.2 (обычно терминируется на ingress), HSTS, CSP, `nosniff`, Referrer/Permissions-Policy, body limit + per-service `WithReadMaxBytes`; CSRF — только bearer-токены + обязательный `Connect-Protocol-Version` |
| Контракт → валидация | Proto contract → Protovalidate → ConnectRPC interceptor: сервисы монтируются **только** через `mountService` (`cmd/server/rpc.go`), который всегда добавляет `connectrpc.com/validate`; тест падает на прямом `New*ServiceHandler` |
| Static analysis | `go vet`, staticcheck (в golangci-lint v2), govulncheck — в `task check` и server-ci |
| Load testing | k6 (`test/load/api.js`: smoke/load/stress с порогами; `task load SCENARIO=…`) |

**События.** Никаких NATS/Kafka/RabbitMQ, пока не доказано, что outbox + in-process bus недостаточны. **Никогда не публиковать событие из request handler'а — писать строку в outbox в той же транзакции.** Таблицы outbox/inbox/idempotency — общие, платформенные (`platform.*`, [ADR-0012](/decisions/0012-shared-platform-messaging-tables)); inbox дедуплицирует по `(consumer, event_id)`. Переход состояния, публикующий событие, защищён в SQL (`WHERE status <> …`), чтобы повтор не писал второй outbox-row. Подробности — [events.md](/events), [ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus).

**Блокировки и кэш.** RedSync/Redlock и любые распределённые блокировки в Valkey запрещены — только Postgres advisory locks. Сбой Valkey не должен ломать корректность (только деградация: нет кэша, rate limit fail-open/closed по политике модуля).

**Auth.** По умолчанию — Keycloak (Apache-2.0), альтернатива — Zitadel (AGPL-3.0 core с v3), Ory не рекомендуется (см. ADR-0001 идентити-провайдера). **Новые проекты выставляют `AEDST_AUTH_FIRST_PARTY_ENABLED=false`.** First-party auth (argon2id, ротация refresh-токенов с CAS и reuse detection) — режим совместимости; **не разрабатывать собственные identity-функции** (MFA, восстановление, social login) — это зона IdP.

**Observability.** Никакого self-hosting Tempo/Loki/Pyroscope по умолчанию. Локально — compose-профиль `observability` (один контейнер `grafana/otel-lgtm`). В проде — Grafana Cloud/SigNoz/HyperDX или другой OTLP-бэкенд. Prometheus `/metrics` — опционально, **выключен по умолчанию**.

**Realtime.** WebSocket-библиотеки в `go.mod` нет. Когда продукту понадобится realtime, кандидат — `coder/websocket`, обёрнутый в `internal/platform/**` (не из `internal/modules/**`), и запись в [decisions.md](/decisions).

Запрещено: ORM вместо sqlc; второй job queue; **вторая JWT/JOSE-библиотека** и свой JWT; свой router; свой validation framework; новый HTTP framework; **Prometheus-специфичная инструментация** (использовать OpenTelemetry API); новый event broker без доказанной необходимости; свой distributed lock; свой rate limiter; RedSync; NATS.

### Android

Kotlin, Jetpack Compose (BOM через `platform()`), Hilt (DI), Coroutines + Flow, AndroidX Navigation, WorkManager, Kotlin Serialization, JUnit/AndroidX/Compose testing. UI — `apps/android/core/ui/`. **JDK 21 обязателен** (detekt 1.23 падает на JDK 25). Перед первой сборкой выполнить `make generate` (buf). Версии — только latest stable.

| Ответственность | Инструмент |
|---|---|
| API-клиент | сгенерированный Protobuf + ConnectRPC (**connect-kotlin 0.9.0**) поверх **OkHttp 5.5.0** (compileSdk 37) — **не Retrofit** |
| DI | Hilt на AGP 9 со встроенным Kotlin (без явного плагина `kotlin-android`) |
| Локальная реляционная БД | Room |
| Типизированные key-value настройки | DataStore + Protobuf |
| Загрузка изображений | Coil |
| Хранение секретов/токенов | Android Keystore + **Tink AEAD** (`AndroidKeysetManager`), зашифрованный DataStore-serializer |
| Offline-очередь записи / фоновая доставка | Room (outbox-таблица) + **WorkManager** (`core/sync`); Hilt-Work для `@HiltWorker` — **не** свой scheduler, не polling, не `AlarmManager` |
| Состояние сети | `ConnectivityManager.registerDefaultNetworkCallback` (платформенный API, `core/network`) — без сторонних библиотек |
| Splash / edge-to-edge | `androidx.core:core-splashscreen` **1.2.0** + `enableEdgeToEdge()` |
| Тесты UI/БД/Worker на JVM | **Robolectric 4.17** + Compose UI test + `room-testing` + `work-testing`; инструментальные a11y-проверки — `ui-test-junit4-accessibility` (ATF, версия из Compose BOM) |
| Эмулятор в CI | GitHub Action `reactivecircus/android-emulator-runner` (только CI, не зависимость приложения) |
| Anti-abuse / device attestation | Play Integrity API |
| Crash reporting / performance / tracing | **Sentry Android** (`sentry-android`, `sentry-okhttp` с `traceparent` только на API-хост, `sentry-android-navigation`, Sentry Gradle plugin для R8 mapping) — как на Web |
| Product-аналитика / feature flags | **PostHog Android** — opt-out до согласия, выключен при пустом ключе |
| Push | **Firebase Cloud Messaging** (`firebase-messaging` через BoM) — Firebase используется **только** для FCM; google-services plugin применяется лишь при наличии `google-services.json` |
| Медиа | **AndroidX Media3** (ExoPlayer + `ui-compose` — плеер; Transformer — сжатие видео), Coil 3 (изображения), Photo Picker (`PickVisualMedia`), `ImageDecoder` (сжатие изображений), WorkManager (загрузка: prepare → presigned PUT → complete) |
| Обновление зависимостей | Gradle-плагины `com.github.ben-manes.versions` + `nl.littlerobots.version-catalog-update` (non-stable отклоняются), `scripts/update-deps.sh` |
| Списки на TV | стандартные Compose lazy lists (`tv-foundation` deprecated) |

**Секреты.** `androidx.security:security-crypto` (`EncryptedSharedPreferences`) **deprecated и удалён — не использовать**. Legacy-данные мигрируют по схеме read → write → delete.

**Сеть.** Единый источник истины — `apps/server/api/proto`. Retrofit + hand-written DTO поверх JSON запрещены. OkHttp остаётся нижним HTTP-engine под connect-kotlin. **Plan B**, если connect-kotlin станет неподдерживаемым: Wire / gRPC-Kotlin (сервер также отдаёт gRPC/gRPC-Web).

**DI.** Hilt утверждён (см. «Explicit rejections»). Metro — только если появится KMP. Koin отклонён (нет compile-time проверок).

Запрещено писать свои: navigation framework, DI framework, coroutine abstraction, image loader, serialization layer, lifecycle framework, HTTP/RPC-клиент вместо сгенерированного ConnectRPC-клиента, криптографию поверх Keystore (использовать Tink).

### Apple

> **Статус верификации:** `apps/apple` не компилировался ни в одном окружении (разработка на Windows). Код и манифесты **прошли только статическое ревью**; приёмкой считается первая сборка в Xcode.

Swift, SwiftUI, Swift Concurrency (async/await, actors), Swift Testing/XCTest. UI — `apps/apple/Core/AedstDesignSystem/`. **Минимальные ОС: iOS/iPadOS 15, macOS 12** (ограничивает зрелость SwiftUI, а не зависимости).

| Ответственность | Инструмент |
|---|---|
| Проект/сборка | **Tuist** (`Project.swift`, `Tuist.swift`, `Tuist/Package.swift`; `tuist install/generate/build/test`; сгенерированный проект в `.gitignore`). XcodeGen **не используется** |
| API-клиент | сгенерированный Protobuf + ConnectRPC (**connect-swift 1.2.3**: `UnaryInterceptor`, `InterceptorFactory`, `ProtoCodec`); swift-protobuf `>= 1.31.0` |
| Локальная персистентность | **GRDB 7** (`DatabasePool` + `DatabaseMigrator`) — **не SwiftData** (нужен iOS 17). SQLiteData (Point-Free) возможен позже поверх GRDB |
| Observation | **swift-perception** (`@Perceptible`, `WithPerceptionTracking`, `@Perception.Bindable`), пин `< 2.0.12`: 2.0.12 объявляет swift-tools-version 6.4, который текущий Xcode не читает |
| Навигация | **FlowStacks** (backport `NavigationStack`), обёрнут в `AedstNavigation` |
| Crash reporting / tracing | **Sentry Cocoa** (dSYM upload в CI, `traceparent` только на API-хост) — как на Web/Android |
| Product-аналитика / feature flags | **PostHog iOS** — opt-out до согласия, выключен при пустом ключе |
| Push | APNs: `UNUserNotificationCenter` + `registerForRemoteNotifications` (платформенный API) |
| Медиа | AVKit (плеер), PHPicker (выбор), ImageIO `CGImageDestination` (сжатие HEIC/JPEG), `AVAssetExportSession` (видео), QuickLookThumbnailing/`AVAssetImageGenerator` (превью), background `URLSession` (загрузка); кэш изображений — `URLCache` по media id (без сторонней библиотеки) |

**Ограничения API (OS-reach).** Не использовать `Duration` / `Task.sleep(for:)` (iOS 16) и `Environment(_: Observable)` (iOS 17); более новые API — только за `#available`. Нативный `@Observable` не использовать там, где нужен iOS < 17 — Perception.

**Plan B** для транспорта — grpc-swift-2 — **отклонён как основной**: требует macOS 15 / iOS 18 и уничтожил бы цель широкого OS-reach.

**Платформенные API вместо библиотек** (новых зависимостей не добавлено): `LocalAuthentication` (блокировка приложения), `Network`/`NWPathMonitor` (доступность сети), `MetricKit` (диагностика), `os.Logger`/`OSSignposter` (логи и профилирование), `Security` (Keychain), `String(localized:)` + String Catalog (локализация). Релиз: `xcodebuild`, `notarytool`, `stapler`, GitHub Actions — **без fastlane** (см. [apple-release](/apple-release)).

Приоритет: Swift Standard Library + Apple Frameworks + утверждённые зависимости выше — вместо собственного framework.

### Desktop

> **Статус верификации:** `apps/desktop` не конфигурировался и не собирался (нет тулчейна). Всё — статически ревьюено; приёмка — первая реальная сборка. Conformance-suite connectrpc — **отложенный follow-up** (не сделан).

Modern C++23, **Qt 6** (официальные бинарники, сейчас 6.12.0; версия — `apps/desktop/qt-version.txt`, единственный источник истины), CMake + CMakePresets, CTest, clang-tidy, clang-format. UI — `apps/desktop/Modules/Core/DesignSystem/`. Правила производительности — `apps/desktop/docs/performance.md`.

Использовать Qt API вместо самодельных аналогов: signals/slots вместо своей event-системы, `QTimer`, Qt filesystem/`std::filesystem`, Qt JSON, Qt Network, Qt concurrency, `std::unique_ptr`/`QPointer`. RAII, const-correctness, move semantics, никаких владеющих raw-указателей.

| Ответственность | Инструмент |
|---|---|
| Управление пакетами | **vcpkg manifest mode** (`vcpkg.json`, `builtin-baseline`) для всего, кроме Qt. Conan **не используется** — [ADR-0002](https://github.com/aedst/aedst/blob/main/apps/desktop/docs/adr/0002-dependency-management.md) |
| API-контракт | **protoc-генерируемые C++ messages** (на этапе сборки, сгенерированный код не коммитится, тот же protoc, что и libprotobuf) + тонкий Connect-protocol клиент `ConnectClient` на Qt Network — [ADR-0001](https://github.com/aedst/aedst/blob/main/apps/desktop/docs/adr/0001-api-contract-client.md) |
| Валидация | vendored protovalidate proto в `third_party/`, пин по `buf.lock` (`scripts/check-protovalidate-pin.sh`, `scripts/check-proto-freshness.sh`) |
| Локальная БД | SQLite (vcpkg `sqlite3`) за RAII-обёрткой |
| Низкоуровневый транспорт вне Qt Network | Boost.Asio — **строго опционально**, флаг `AEDST_ENABLE_ASIO`, по умолчанию `OFF` |
| Хранение секретов | Платформенные API, без новых зависимостей: DPAPI (Windows), Security.framework Keychain (macOS), libsecret (Linux, опциональный системный пакет `libsecret-1-dev` через pkg-config); AES-256-GCM через OpenSSL EVP — [ADR-0003](https://github.com/aedst/aedst/blob/main/apps/desktop/docs/adr/0003-secret-storage.md) |
| Доступность сети / i18n | `QNetworkInformation`; `QTranslator` + `qt_add_translations` (Qt Linguist), собственный код — только выбор языка и RTL (`TranslationLoader`) |
| Платформенные сервисы | `QLockFile`/`QLocalServer` (single instance), `QtDBus` (опционально, уведомления Linux), автозапуск через `QSettings`/`QSaveFile`; `QSystemTrayIcon` — нативные уведомления (desktop не имеет FCM/APNs: непрочитанные опрашиваются с backoff), поэтому линкуется Qt Widgets — ADR-0022 частично заменяет [ADR-0006](https://github.com/aedst/aedst/blob/main/apps/desktop/docs/adr/0006-platform-services-and-updates.md) |
| Обновления | Ed25519 + SHA-256 через OpenSSL EVP |
| Крэши / ошибки | **sentry-native** (vcpkg, crashpad backend, `AEDST_ENABLE_SENTRY`); без DSN — локальный crash handler (`dbghelp` / `sigaction`) |
| Product-аналитика / feature flags | PostHog HTTP API через `QNetworkAccessManager` (без SDK), opt-in |
| Медиа | Qt Multimedia (`MediaPlayer` + `VideoOutput`, `QVideoSink` для превью), `QImageReader`/`QImageWriter` (сжатие), `QQuickImageProvider` с кэшем |
| Транспортная политика | Только Qt API (`QSslConfiguration`, pinning по SPKI SHA-256); `ignoreSslErrors` запрещён — [ADR-0004](https://github.com/aedst/aedst/blob/main/apps/desktop/docs/adr/0004-transport-policy.md) |
| Тесты / анализ / релиз | GoogleTest (`unit`), Qt Test (`qttest`), Qt Quick Test (`qmltest`); ASan/UBSan/TSan (`AEDST_SANITIZER`), gcovr (только CI), qmllint/qmlformat из Qt, clang-tidy (блокирующий, baseline); CPack (NSIS/ZIP, DEB/RPM/TGZ, DragNDrop/productbuild), `signtool`, `codesign`, `notarytool`/`stapler`, `objcopy`/`dsymutil` для символов |

**Custom-code exception (задокументировано, ADR-0001).** Тонкий Connect-клиент — собственный код, потому что готового Connect C++ клиента не существует, а gRPC C++ дублирует HTTP/TLS/proxy-стек Qt и обходит системное хранилище сертификатов ОС. Правила его расширения:

- Новый RPC = запись в `contract/methods.hpp` (тест на дрейф дескрипторов упадёт без неё).
- Ретраи — только для методов, объявленных `Idempotent`.
- Client/bidi streaming **не поддерживаются**; при появлении такой потребности — пересмотреть gRPC (см. [decisions.md](/decisions)).

**Правила производительности (`docs/performance.md`):** LTO в Release; compiler cache; mold/lld; QML компилируется ahead-of-time через `qt_add_qml_module` (обычный `qt_add_resources` для production-QML запрещён); `QAbstractItemModel` с ролями, а не JS-массивы; один долгоживущий `QNetworkAccessManager`; никогда не поставлять `QT_QML_DEBUG`; трассировка «startup +N ms» до/после изменений; `QBENCHMARK` для горячих путей; PCH/mimalloc/unity build — только при измеренном выигрыше.

**Правило по Boost.Asio:** включать `AEDST_ENABLE_ASIO=ON` разрешено только под задокументированную причину (Qt Network не покрывает сценарий, напр. сырой TCP/UDP без HTTP-семантики). **Запрещено** включать Asio ради того, что умеют `QNetworkAccessManager`/`QTcpSocket`/`QWebSocket`.

**Два логгера и два тест-фреймворка — сознательное решение (см. «Explicit rejections»).** `AedstLogger` (spdlog) — для Qt-независимого кода и GoogleTest-бинарников; `QLoggingCategory` — для Qt/QML-диагностики. GoogleTest/gMock — чистый C++, Qt Test — QObject/ViewModels. Не создавать третий логгер/фреймворк.

## Explicit rejections (do not re-propose)

Владелец проекта **осознанно решил оставить** следующее. AI-агент не предлагает замену, миграцию или «улучшение» без явного запроса владельца. Полный список с обоснованием и условиями пересмотра — [decisions.md](/decisions).

| Что оставляем | Вместо чего не предлагать | Мотив (кратко; первичен запрос владельца) |
|---|---|---|
| **Next.js** (Web) | Vite / другой SPA-стек | решение владельца; App Router, next-intl, Vercel-интеграции уже построены на нём |
| **react-use** | самописные хуки, замена на usehooks-ts и т.п. | один primary-инструмент для общих React-хуков |
| **React Hook Form** | TanStack Form, Formik | принятый стандарт форм; связка с Zod и shadcn/ui |
| **`cn` (npm-пакет)** | ручной `clsx + tailwind-merge` хелпер | зависимость shadcn-экосистемы |
| **Chi** (Server) | Echo/Gin/Fiber/stdlib-only router | шаблон проекта; экосистема middleware |
| **Goose** (Server) | Atlas, golang-migrate | Atlas платный; Goose проверен и хорошо ложится на модульный `goose_<module>` |
| **Qt 6 / C++23** (Desktop) | Electron, Tauri, Flutter, .NET/WinUI | осознанный выбор нативного стека |
| **Оба логгера** (spdlog + Qt) и **оба тест-фреймворка** (GoogleTest + Qt Test) | «унифицировать» до одного | разделение по слоям (Qt-free vs Qt-aware), см. Desktop |
| **Hilt** на AGP 9 (Android) | Koin, ручной DI | compile-time проверки; Metro — только при появлении KMP |

## Monorepo tooling

> Эти файлы создаются параллельно другими участниками работ в корне репозитория: `mise.toml`, `Taskfile.yml`, `packages/design-tokens/`. Если файла нет в вашей ветке — он ещё не влит; ссылайтесь на него как на целевую конфигурацию, а не выдумывайте содержимое.

- **mise** (`mise.toml` в корне) пинит версии тулчейнов (Node, pnpm, Go, JDK 21, buf, task и т.д.). Версию тулчейна меняют **только там**; не дублируйте её в README/CI без ссылки на `mise.toml`. Исключение: Qt для Desktop — `apps/desktop/qt-version.txt`.
- **Taskfile** — единая точка входа. Корневой `Taskfile.yml` подключает Taskfile каждого приложения через `includes`. Агент запускает `task <namespace>:<task>` (например, `task lint:migrations` на сервере), а не изобретает собственные команды/скрипты, если для этого уже есть задача. Новая рутина = новая задача в Taskfile соответствующего приложения.
- **Design tokens** — единый источник в `packages/design-tokens/` (Style Dictionary). Из него **генерируются** токены для Web, Android, Apple и Desktop. **Сгенерированные файлы нельзя править вручную** — правится источник, затем запускается генерация (`task` для design-tokens). Правка сгенерированного файла в PR — дефект. Подробнее о UI-ките — [ui-kit.md](/ui-kit).

### Задокументированные исключения (Web)

Подтверждено технической причиной по правилам выше:

- `shared/lib/hooks/use-debounced-value.ts` — `debounce` из nuqs откладывает только запись в URL, а состояние, от которого зависит запрос, обновляется сразу; нужен debounce значения, питающего запрос.
- `shared/lib/hooks/use-hydrated.ts` — форма не должна отправляться нативно до гидрации (пароль в URL); готового решения в утверждённом стеке нет.
- `shared/lib/consent/*` — согласие должно читаться сервером (cookie), чтобы баннер попадал в первый HTML; доменная логика проекта.
- `app/api/rpc/[...path]/route.ts` — BFF-прокси с refresh: требование безопасности (токены не в JS).

## One Responsibility → One Primary Tool

Для одной ответственности — один основной инструмент, если нет явной архитектурной причины иметь несколько. Не `Moment + date-fns + Day.js + Luxon` одновременно — только `date-fns`. То же правило действует на Backend, Android, Apple, Desktop.

## Custom Code Exception

Собственный код разрешён только после прохождения всей цепочки (Standard Library → BOLIPLARE Library → Platform API → Approved Library → Existing Internal Implementation) и только с технической причиной:

**Хорошие причины:** функциональность недоступна; библиотека не поддерживает платформу; несовместимый API; неприемлемые характеристики производительности; требование безопасности/compliance; доменная логика проекта; зависимость внесла бы значительно больше сложности, чем сама реализация; библиотека заброшена/несовместима.

**Плохие причины:** "мне проще написать самому", "это всего 50 строк", "я умею сделать лучше", "мне не нравится API", "так быстрее написать".

## Existing Code First

Перед созданием нового компонента/сервиса/utility искать: существующую реализацию → существующую абстракцию → существующий модуль → существующую зависимость → существующий API → существующий паттерн → существующий тест.

- Подходит → **REUSE**
- Почти подходит → **EXTEND**
- Архитектурно неправильный → **REFACTOR**
- Ничего из этого невозможно → **CREATE**

## Dependency Addition Rule

Если существующий stack не решает задачу и нужна новая зависимость: search ecosystem → проверить официальную библиотеку → maintenance status → лицензию → security history → compatibility → bundle/build impact → transitive dependencies → может ли решить существующая зависимость → добавлять только при обоснованности. Не добавлять библиотеку только потому, что она популярна.

## Anti-patterns (примеры)

```text
❌ Не писать собственный HTTP client        → использовать существующий API client
❌ Не писать собственную form validation    → React Hook Form + Zod
❌ Не писать собственный date formatter     → date-fns
❌ Не писать собственный drag & drop        → dnd-kit
❌ Не писать собственный JWT                → jwx
❌ Не писать собственный env parser         → caarlos0/env
❌ Не писать собственный job queue          → River
❌ Не писать собственный WebSocket          → coder/websocket
❌ Не писать собственный distributed lock   → Postgres advisory locks (internal/platform/lock)
❌ Не публиковать события из handler-а      → transactional outbox
❌ Не писать собственные identity-функции   → OIDC IdP (Keycloak по умолчанию)
❌ Не править сгенерированные design tokens → packages/design-tokens (источник)
❌ Не писать версию зависимости по памяти   → проверить в реестре (Version policy)
❌ Не писать собственную protobuf validation → Protovalidate
❌ Не писать собственный JSON/protobuf serializer → стандартный/официальный serializer
❌ Не писать собственную crypto             → проверенные криптографические библиотеки
❌ Не писать собственный UI component       → сначала проверить shadcn/ui / core/ui / AedstDesignSystem
```

AI должен сам обнаруживать: custom-код, дублирующий существующую библиотеку; дублирующиеся utilities/dependencies/API-клиенты/validation/state-management/networking/caching/retry-логику/сериализацию/date-handling/UI-примитивы/auth-логику. При обнаружении: **STOP → REVIEW → REUSE/CONSOLIDATE → TEST**.

## Внедрение (enforcement)

Документации недостаточно — правила проверяются автоматически там, где это возможно:

| Платформа | Инструмент |
|---|---|
| Web | Biome (единственный линтер/форматтер), TypeScript, knip (`apps/web/knip.jsonc`), `scripts/check-feature-boundaries.sh` |
| Server | golangci-lint (`depguard`), `go vet`, `go mod tidy` diff, `architecturetest`, `sqlc vet`, squawk (`task lint:migrations`) |
| Android | Detekt (JDK 21), ktlint |
| Apple | SwiftLint (первая проверка — при первой сборке в Xcode) |
| Desktop | clang-format, clang-tidy, `check-proto-freshness.sh`, `check-protovalidate-pin.sh`, тест дрейфа `contract/methods.hpp` |
| Все | dependency scanners, CI, `scripts/check-naming.sh` |

Наименование файлов/компонентов/зависимостей — см. [CONVENTIONS.md](https://github.com/aedst/aedst/blob/main/CONVENTIONS.md). Список принятых и отвергнутых решений — [decisions.md](/decisions).

## Definition of Done для AI

```text
[ ] Existing implementation checked
[ ] Existing dependencies checked
[ ] Platform API checked
[ ] No unnecessary custom implementation
[ ] No duplicate dependency
[ ] Architecture rules respected
[ ] Naming conventions respected (CONVENTIONS.md)
[ ] Tests added/updated
[ ] Linter passes
[ ] Formatter passes
[ ] Type checking passes
[ ] Build passes
[ ] Existing tests pass
[ ] Security checks pass where applicable
[ ] Documentation updated where architecture changed
[ ] New dependency: allowlist (knip / depguard / манифест) + таблица в library-first.md обновлены
[ ] Версии проверены по реестру, не выдуманы; непроверенное помечено UNVERIFIED
[ ] Не предложена замена из «Explicit rejections»
```

## Философия

```
Reuse > Rewrite
Existing abstraction > New abstraction
Platform API > Custom implementation
Established library > Handwritten utility
One standard tool > Multiple overlapping tools
Simple architecture > Clever architecture
Explicit dependency > Hidden dependency
Tested implementation > Fast implementation
Production solution > Prototype solution
```

**Финальное правило:** не пиши код, который уже умеет делать существующая библиотека или платформа. Сначала найди и используй существующее решение. Только после доказанной невозможности reuse разрешается создавать собственную реализацию.
