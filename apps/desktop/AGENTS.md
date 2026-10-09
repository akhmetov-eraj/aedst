# apps/desktop - AI agent instructions

Read the root [AGENTS.md](../../AGENTS.md) and [CONVENTIONS.md](../../CONVENTIONS.md) first; this file only adds
what is specific to the Qt 6 / C++23 app. Reference module for everything below: `Modules/Features/Tasks`.
Approved libraries: [library-first/desktop](../../docs/content/library-first/desktop.md). Decisions:
[decisions/desktop](../../docs/content/decisions/desktop.md), ADRs in `docs/adr/`.
Naming: [conventions/desktop.md](../../docs/agent/conventions/desktop.md). Local rules: `CODING_STANDARDS.md`, `CONTRIBUTING.md`.

Verification status: the app has not been configured or compiled where this was written (no Qt, vcpkg or compiler).
Only the grep-based checks run anywhere. Do not claim it builds ([status](../../docs/content/status.md)).

## Pairs: when you do X, use Y

| When you add | Use | Copy / read | Test |
|---|---|---|---|
| A screen | A QML `*View.qml` in the feature's `Presentation/` (stateless renderer) plus a `QObject` view model with `Q_PROPERTY`; QML modules are compiled ahead of time via `qt_add_qml_module`, never plain `qt_add_resources` | `Presentation/TaskListView.qml`, `task_list_view_model.hpp` | GoogleTest for the view model; Qt Quick Test (`qmltest`) for QML |
| A list | A `QAbstractListModel` with roles, never a JS array | `Presentation/task_list_model.hpp` | Qt Test (`qttest`) |
| A route | A route struct and a destinations registration; `Router` and `NavGraph` live in `Core/Navigation` | `Navigation/tasks_route.hpp`, `tasks_destinations.cpp` | view-model test |
| An RPC call | The `ConnectClient` (Qt Network) behind a repository interface in `Domain/`, implementation in `Data/`. A new RPC must first get an entry in `Modules/Core/Contract/include/aedst/contract/methods.hpp` (a drift test fails without it); retry only `Idempotent` methods | `Data/connect_task_repository.cpp`, `Domain/task_repository.hpp` | `tests/create_task_use_case_test.cpp` with a fake repository |
| A write that must survive offline | An `OutboxHandler` registered with the outbox service; send the item id as `Idempotency-Key` | `Data/task_outbox_handlers.cpp`, `Core/Outbox/include/aedst/outbox/outbox_handler.hpp` | `tests/task_outbox_handlers_test.cpp` |
| A setting | `QSettings` through a small settings class in the owning module; secrets only through `Core/Platform` secure storage (DPAPI / libsecret) and QtKeychain, never a plain file | `Core/Telemetry/.../telemetry_settings.hpp` | unit test with a temporary `QSettings` scope |
| A UI element | `Modules/Core/DesignSystem/qml` components first (`AedstLabel`, `AedstListItem`, ...); never a raw `Label/Text/Button/ItemDelegate` in a feature; interactive controls need `Accessible.role` + `Accessible.name` | `Core/DesignSystem/qml` | `check-qml-components.sh`, `check-qml-accessibility.sh`, `qmllint` |
| An object QML reaches as a singleton | `QML_ELEMENT` + `QML_SINGLETON` + `static create()` via `SingletonSlot<T>` (`Core/QmlSupport`), instance supplied in `App/src/qml_singletons.cpp`; never `qmlRegister*` | `Core/Navigation/.../router.hpp` | `check-forbidden-apis.sh` |
| A color, spacing, font | Design tokens (`AedstTokens.qml`, `AedstLightPalette.*` are generated); no hex literals in QML or C++ | `Core/DesignSystem` | `task check:colors` (report-only on desktop) |
| A user-facing string | `qsTr()` in QML, `tr()` in C++, and the five `i18n/<module>_{en,ru,tg,uz,ar}.ts` files (Qt Linguist, `lupdate` target `update_<module>_translations`) | `Modules/Features/Tasks/i18n/` | `task check:i18n` |
| An analytics event | A constant in `Core/Analytics/.../analytics_tracker.hpp` `events::` and a call on the injected `AnalyticsTracker` | `task_list_view_model.cpp` | fake tracker in the test |
| An error shown to the user | Report to the injected `core::ErrorSink`; PII scrubbing is `core::scrubPii` | `task_list_view_model.hpp` | view-model test |
| A logger line | `QLoggingCategory` (`aedst.*`) in Qt code; spdlog only in Qt-independent code. No third logger | `Core/Logging` | n/a |
| A feature module | `Modules/Features/<Name>` with `Domain`, `Data`, `Presentation`, `Navigation`, `i18n`, `tests`, a `CMakeLists.txt`; link `Aedst::*` Core targets only, never `Aedst::Feature::*`; add it to the root `CMakeLists.txt` and `App/CMakeLists.txt` | `Modules/Features/Tasks/CMakeLists.txt` | `scripts/check-feature-boundaries.sh` |

## Rules that bite

- C++23 subset only: no `std::print`, `std::format`, `std::flat_map`, `std::generator`, deducing `this`, `import std`
  and similar (`scripts/check-cxx-subset.sh`; the list is in `CODING_STANDARDS.md`).
- Qt APIs before custom code; RAII, no owning raw pointers, `QPointer` or `std::unique_ptr`. Forbidden APIs (`ignoreSslErrors`,
  `QThread` subclasses, stray `QNetworkAccessManager`/`std::thread`, bare `qDebug`, `qmlRegister*`, unpinned `FetchContent`) are
  gated by `scripts/check-forbidden-apis.sh`; the escape hatch is `// aedst-allow: <reason>`.
- Rejected: Conan, hand-mirrored DTOs, committed generated code, RPCs missing from `contract/methods.hpp`,
  shipping `QT_QML_DEBUG`. Protobuf messages are generated by `protoc` at build time.
- Generated and read-only: `docs/generated/**`, `**/generated/**`, `AedstTokens.qml`, `AedstLightPalette.*`,
  `AedstDarkPalette.*`. A PreToolUse hook refuses edits.
- Qt version lives in `qt-version.txt` only. Files stay at or under 300 lines (over 500 fails `task check:file-size`).

## Gates (run before saying done)

1. `task desktop:lint` (feature boundaries, proto freshness, protovalidate pin), `bash scripts/check-cxx-subset.sh`
   and `bash scripts/check-naming.sh` from `apps/desktop`; `scripts/check-format.sh` when clang-format is installed.
   Also `check-forbidden-apis.sh`, `check-qml-components.sh`, `check-qml-accessibility.sh`, `check-migration-hashes.sh`,
   `node scripts/check-i18n-extract.mjs`, and `bash scripts/selftest/run-selftests.sh` (every gate fails on its fixture).
2. `task check` from the repo root.
3. With Qt, vcpkg and a compiler: `task desktop:test` (configure, build, ctest) or `bash scripts/verify-desktop-local.sh`.
4. If step 3 did not run, say that nothing was compiled or tested.
