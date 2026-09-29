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

## Обязательный workflow

```
TASK → UNDERSTAND REQUIREMENT → CHECK EXISTING CODE
     → CHECK EXISTING BOLIPLARE LIBRARIES → CHECK PLATFORM STANDARD API
     → CHECK OFFICIAL / RECOMMENDED LIBRARY → CHECK EXISTING INTERNAL UTILITY
     → REUSE
     → (только если ничего не подошло) → IMPLEMENT CUSTOM CODE
```

Перед созданием utility/helper/hook/component/service/repository/parser/formatter/validator/serializer/cache/queue/retry/HTTP-client/WebSocket-layer/auth-mechanism/crypto/date-time/file-handling/image-processing/drag-and-drop/state-management/animation/testing-utility — обязательно пройти Dependency Discovery: стандартная библиотека → BOLIPLARE stack → установленные зависимости → официальный API платформы → рекомендуемая ecosystem-библиотека → существующая реализация в проекте.

## Approved stack по платформам

### Web

| Ответственность | Инструмент |
|---|---|
| Forms | React Hook Form |
| Validation | Zod |
| Dates | date-fns |
| Drag & Drop | dnd-kit |
| UI | shadcn/ui (`apps/web/src/shared/ui/`) |
| Animations | Motion |
| URL State | nuqs |
| React utilities | React Use |
| Server State | TanStack Query |
| Tables | TanStack Table |
| Virtualization | TanStack Virtual |
| Testing | Vitest / Playwright |
| API | сгенерированный Protobuf + ConnectRPC клиент |
| Reactive streams / сложная композиция событий | RxJS |
| AI/LLM интеграция (стриминг чата, tool calls, structured output) | Vercel AI SDK (`ai`) |
| Дополнительные React-хуки общего назначения (debounce, media query, local storage и т.п.) | React Use |
| Error tracking / crash reporting | Sentry (`@sentry/nextjs`) |
| Product/performance аналитика | Vercel Analytics + Speed Insights |
| i18n | next-intl |
| Графики | Recharts |
| Command palette | cmdk |
| Карусель | Embla Carousel |
| Date picker | react-day-picker |
| OTP-инпут | input-otp |
| Resizable-панели | react-resizable-panels |
| Тосты/уведомления | Sonner |
| Переключение темы (light/dark) | next-themes |

**Примечание:** RxJS, Vercel AI SDK, React Use, Sentry, Vercel Analytics/Speed Insights, next-intl и перечисленные ниже shadcn/ui-экосистемные библиотеки (Recharts, cmdk, Embla Carousel, react-day-picker, input-otp, react-resizable-panels, Sonner, next-themes) утверждены как approved stack для Web **до того, как в коде появится первое использование** — эти интеграции запланированы (AI-функциональность, observability, i18n, богатые shadcn-компоненты) и добавление кода под них не требует повторного прохождения Dependency Discovery. Каждая library всё равно даёт ровно одну ответственность (см. "One Responsibility → One Primary Tool") — не добавлять вторую библиотеку на ту же роль без документированной причины. Если зависимость добавлена в `package.json`, но не импортируется нигде в `src/` дольше одного релизного цикла — считать её нарушением "Existing Code First" и либо подключить, либо удалить (dependency scanner должен это ловить).

### Server (Go)

| Ответственность | Инструмент |
|---|---|
| Router | Chi |
| API Contract | Protobuf |
| API Generation | Buf |
| RPC | ConnectRPC |
| Validation | Protovalidate |
| Database | PostgreSQL |
| Driver | pgx |
| SQL | sqlc |
| Migrations | Goose |
| Jobs | River |
| Events | NATS JetStream |
| Cache | Redis |
| Distributed Locks | RedSync |
| Rate Limiting | redis_rate |
| WebSocket | coder/websocket |
| JWT / JOSE | jwx |
| Configuration | caarlos0/env |
| Observability | OpenTelemetry / Prometheus / Tempo / Loki / Pyroscope |
| Integration Testing | Testcontainers |

**Примечание:** `coder/websocket` — прямая (не indirect) зависимость в `go.mod`, approved под запланированные realtime-эндпоинты, даже до появления первого `websocket.Accept(...)` в коде. При первом реальном использовании — оборачивать в `internal/platform/**` (по аналогии с `internal/platform/messaging` для NATS), не звать напрямую из `internal/modules/**`.

Запрещено: ORM вместо sqlc, новый job queue вместо River, свой JWT, свой router, свой validation framework, новый HTTP framework, новый event broker без необходимости, свой distributed lock, свой rate limiter.

### Android

Kotlin, Jetpack Compose, Hilt (DI), Coroutines + Flow, AndroidX Navigation, WorkManager, Kotlin Serialization, JUnit/AndroidX/Compose testing. UI — `apps/android/core/ui/` (см. его README).

| Ответственность | Инструмент |
|---|---|
| API-клиент | сгенерированный Protobuf + ConnectRPC (connect-kotlin) клиент — **не Retrofit/OkHttp** |
| Локальная реляционная БД | Room |
| Типизированные key-value настройки | DataStore + Protobuf |
| Загрузка изображений | Coil |
| Хранение секретов/токенов | androidx.security-crypto (EncryptedSharedPreferences) |
| Anti-abuse / device attestation | Play Integrity API |
| Crash reporting / performance / product-аналитика | Firebase (Crashlytics, Performance, Analytics) |

**Важно — унификация с остальными платформами:** сеть на Android строится на сгенерированном ConnectRPC/Protobuf-клиенте (`connect-kotlin` + `protoc-gen-connect-kotlin`), тем же паттерном, что уже используют Web (`@connectrpc/connect`) и Apple (`connect-swift`) — единый источник истины `apps/server/api/proto`, никакого дублирования контракта через отдельный REST/JSON DTO-слой. Retrofit + OkHttp как транспорт для вызовов `aedst.*.v1.*Service` **запрещены** — это архитектурное расхождение с остальными клиентами, а не просто "другая библиотека". OkHttp может остаться нижним HTTP-engine'ом под `connect-kotlin` (сам connect-kotlin использует pluggable `HTTPClientInterface`, включая OkHttp-реализацию) — запрещён именно слой Retrofit + hand-written DTO поверх JSON.

Room, DataStore+Protobuf, Coil, androidx.security-crypto, Play Integrity и Firebase-набор — approved platform-native выбор для локальной персистентности, кэша изображений, безопасного хранения токенов, защиты от абузов и observability/аналитики соответственно; ни один из них не дублирует другой в рамках "One Responsibility → One Primary Tool".

Запрещено писать свои: navigation framework, DI framework, coroutine abstraction, image loader, serialization layer, lifecycle framework, HTTP/RPC-клиент поверх Retrofit вместо сгенерированного ConnectRPC-клиента.

### Apple

Swift, SwiftUI, Observation, Swift Concurrency (async/await, Task, actors), Swift Testing/XCTest. UI — `apps/apple/Core/AedstDesignSystem/`.

| Ответственность | Инструмент |
|---|---|
| API-клиент | сгенерированный Protobuf + ConnectRPC (`connect-swift`) клиент |
| Локальная персистентность | SwiftData |

SwiftData — approved platform-first выбор для локальной персистентности (`Core/AedstPersistence`), согласован с приоритетом "Platform API > custom code": это нативный Apple framework, а не сторонняя зависимость.

Приоритет: Swift Standard Library + Apple Frameworks + BOLIPLARE Libraries — вместо собственного framework.

### Desktop

Modern C++23, Qt 6, CMake, CTest, clang-tidy, clang-format. UI — `apps/desktop/Modules/Core/DesignSystem/`.

Использовать Qt API вместо самодельных аналогов: `Qt signals/slots` вместо своей event-системы, `QTimer` вместо своего таймера, `std::filesystem`/Qt filesystem APIs, Qt JSON, Qt Network, `std::thread`/Qt concurrency, `std::unique_ptr`/`std::shared_ptr`/`QPointer`. Modern C++ обязателен: RAII, const-correctness, move semantics, никаких лишних владеющих raw-указателей.

| Ответственность | Инструмент |
|---|---|
| Локальная БД | SQLite (через conan `sqlite3`, версия пинится в `conanfile.py`) |
| Низкоуровневый сетевой транспорт вне того, что покрывает Qt Network (напр. кастомные бинарные TCP-протоколы, вне HTTP/WebSocket) | Boost.Asio — **строго опционально**, флаг `AEDST_ENABLE_ASIO` (conan-опция `with_asio`), по умолчанию `OFF` |

**Правило по Boost.Asio:** зависимость всегда резолвится в графе (`self.requires("boost/1.86.0")`), но линкуется только когда `AEDST_ENABLE_ASIO=ON`. Включать флаг разрешено только под задокументированную техническую причину по Custom Code Exception — Qt Network не покрывает нужный сценарий (например, сырой TCP/UDP-протокол без HTTP-семантики). **Запрещено** включать Asio, чтобы продублировать то, что уже умеет `QNetworkAccessManager`/`QTcpSocket`/`QWebSocket` — это прямое нарушение "не писать свой HTTP-фреймворк вместо Qt API". Любой PR, включающий `with_asio=True`, должен явно объяснить в описании, почему Qt Network не подошёл.

**Согласованное исключение — логирование:** `Modules/Core/Common/include/aedst/core/logger.hpp` оборачивает **spdlog**, а не `QLoggingCategory`. Причина: код Domain/Data-слоёв и юнит-тесты (GoogleTest-бинарники) должны логировать без запущенного `QCoreApplication`/`QGuiApplication`, а `QLoggingCategory` требует Qt event loop и Qt-типы. `qDebug`/`QLoggingCategory` остаются единственным инструментом для Qt/QML-специфичной диагностики (biндинги, рендеринг, сигналы) — два логгера не дублируют, а разделяют ответственность по слоям. Валидная причина по правилам Custom Code Exception (несовместимый API платформы для Qt-независимого кода). Не создавать третий логгер поверх этих двух.

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
❌ Не писать собственную protobuf validation → Protovalidate
❌ Не писать собственный JSON/protobuf serializer → стандартный/официальный serializer
❌ Не писать собственную crypto             → проверенные криптографические библиотеки
❌ Не писать собственный UI component       → сначала проверить shadcn/ui / core/ui / AedstDesignSystem
```

AI должен сам обнаруживать: custom-код, дублирующий существующую библиотеку; дублирующиеся utilities/dependencies/API-клиенты/validation/state-management/networking/caching/retry-логику/сериализацию/date-handling/UI-примитивы/auth-логику. При обнаружении: **STOP → REVIEW → REUSE/CONSOLIDATE → TEST**.

## Внедрение (enforcement)

Документации недостаточно — правила должны проверяться автоматически там, где это возможно:

| Платформа | Инструмент |
|---|---|
| Web | Biome, ESLint/TypeScript |
| Android | Detekt, ktlint |
| Apple | SwiftLint |
| Desktop | clang-format, clang-tidy |
| Server | golangci-lint, go vet |
| Все | dependency scanners, CI |

Наименование файлов/компонентов/зависимостей — см. [CONVENTIONS.md](https://github.com/aedst/aedst/blob/main/CONVENTIONS.md) и его `scripts/check-naming.sh`.

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
