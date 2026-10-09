# Naming: Desktop (C++ / Qt, `apps/desktop`)

Part of [CONVENTIONS.md](../../../CONVENTIONS.md). Levels: **Must** = enforced (linter/CI), **Should** = needs a one-line PR justification to deviate, **Could** = preference.

| What | Rule | Example | Why | Level |
|---|---|---|---|---|
| File (`.cpp`/`.hpp`/`.h`) | `snake_case` | `task_repository.cpp`, `task_repository.hpp` | Google C++ Style Guide (the base convention for `clang-tidy` here); grep-friendly, no case ambiguity between Windows/Linux filesystems in one repo | Must |
| Class / struct / enum type | `PascalCase` | `class TaskRepository`, `enum class TaskStatus` | Google C++ Style Guide; separates types from functions/variables when reading | Must |
| Functions / methods | `camelCase` | `void loadTasks()`, `bool isOverdue() const` | Qt convention (the whole Qt API is `camelCase`: `QObject::setParent`); the project is built on Qt, so methods follow Qt rather than Google style | Must |
| Local variables / member variables | `camelCase`; members use a trailing `_`, not an `m_` prefix | `int retryCount; int retryCount_;` (we choose the **`_` suffix**) | Trailing underscore is Google C++ style, consistent with the Google-based clang-tidy config; works with `readability-identifier-naming` without special cases for `m_` | Should |
| Signals / slots (Qt) | `camelCase`; a slot is a verb, a signal is a past-tense event or the "changed" pattern | `signals: void taskCompleted(); public slots: void onTaskCompleted();` | Standard Qt convention; `on*` for slots reacting to a signal is the common Qt idiom | Should |
| Constants | `kPascalCase` | `constexpr int kMaxRetryCount = 3;` | Google C++ Style Guide: the `k` prefix unambiguously marks a compile-time constant in any context | Must |
| `namespace` | `lowercase` | `namespace aedst::tasks { ... }` | Google C++ Style Guide; matches the Go/Kotlin/Android package case, so "module/namespace = lowercase" holds across ecosystems except Swift | Must |
| `.ui` files (Qt Designer) | `snake_case` | `task_details_widget.ui` | Matches the other module files; the generated `ui_task_details_widget.h` is predictable from the name | Must |
| Header guards | `#pragma once` (not include-guard macros) | `#pragma once` as the first line | Supported by all project compilers (MSVC/Clang/GCC); no macro name to choose, less room for error | Should |
| Test files | `<subject>_test.cpp` | `logger_test.cpp` | Consistent with Go (`_test.go`); the `*_test.*` pattern is recognizable in all five stacks | Should |

`apps/desktop/**` was migrated to `snake_case.cpp/.hpp` before the first commit of each module (there was no `git blame` history, so a mass rename was safe and needed no file-by-file migration). `check-naming.sh` checks this as `err`, not `warn`.

## Anti-patterns

```cpp
// BAD TaskRepository.cpp / TaskRepository.hpp         // files are snake_case: task_repository.cpp/.hpp
// BAD class task_repository { ... }                    // class/type is PascalCase: TaskRepository
// BAD void LoadTasks();                                 // method is camelCase (Qt style): loadTasks()
// BAD constexpr int MAX_RETRY_COUNT = 3;                 // constant is kPascalCase: kMaxRetryCount
// BAD namespace Aedst::Tasks { ... }                     // namespace is lowercase: aedst::tasks
```

## Enforcement gates (apps/desktop)

Beyond naming, these scripts run in `desktop-ci` and fail on the rule ids listed in [library-first/desktop](../../content/library-first/desktop.md#enforced-rules-one-concern-one-mandatory-library-every-gate-has-a-fixture-that-must-fail): `check-forbidden-apis.sh`, `check-qml-components.sh`, `check-qml-accessibility.sh`, `check-migration-hashes.sh`, `check-i18n-extract.mjs`, `check-feature-boundaries.sh`. Exceptions need `// aedst-allow: <reason>`; fixtures are proven by `scripts/selftest/run-selftests.sh`.

## Cross-platform vocabulary (same concept, same name on every client)

| Concept | Desktop name | Rule |
|---|---|---|
| Repository implementation | `<Transport><Name>Repository` in `Data/connect_<name>_repository.*` (`ConnectTaskRepository`, `ConnectAuthRepository`; OIDC sign-in is `OidcAuthRepository`). No `*Impl` suffix | Should |
| Outbox delivery | `OutboxHandler::deliver(OutboxDelivery, DeliveryCompletion)` | Should |
| Capability check | `CapabilitiesService::isEnabled(id)` (the Android `CapabilityResolver.isEnabled` equivalent); wire type `ServerCapabilities` | Should |
| Forced upgrade | `ConnectClient::clientUpgradeRequired()` signal plus `CapabilitiesService::upgradeRequired()` (the `ClientUpgradeInterceptor` equivalent; Desktop has no interceptor class, `ConnectClient` raises it) | Should |
| Behaviour replay tests | `<subject>_replay_test.cpp`, loading `docs/contract/behavior/*.json` through `aedst/testing/behavior_fixture.hpp` (`BehaviorFixture`); the `features.yaml` evidence paths point at them | Should |
| Analytics event names | The web vocabulary in `Core/Analytics/.../analytics_tracker.hpp` (`task_created`, `task_completed`, `notification_opened`); `docs/contract/analytics-events.json` once it exists | Should |
| Route helpers | `navigation::routes::taskDetail(id)` etc. next to `routes::k*`; link prefixes come from `plugin::RouteContribution::linkPrefixes` (see `docs/content/desktop/architecture.md`) | Should |
