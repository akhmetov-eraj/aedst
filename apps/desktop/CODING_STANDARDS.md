# Coding standards — C++ and Qt (AEDST Desktop)

Naming lives in the repo-wide [CONVENTIONS.md](../../CONVENTIONS.md) (files
`snake_case`, `_test.cpp`; enforced by `.clang-tidy` and `scripts/check-naming.sh`).
Dependency choice lives in [library-first.md](../../docs/content/library-first.md):
never write what the standard library, Qt, or an approved dependency already does.

## C++

- C++23, no compiler extensions. Prefer `std::expected` via `core::Outcome<T>`
  for expected failures; exceptions are not used for control flow.
- RAII everywhere; no owning raw pointers; `unique_ptr` by default,
  `shared_ptr` only where ownership is truly shared (DI graph).
- `const`, `[[nodiscard]]` on results that carry errors, `noexcept` where true.
- No blocking work on the GUI thread beyond the measured budgets in
  [ADR-0005](docs/adr/0005-local-database-and-threading.md).
- Warnings are part of the API: the set in `cmake/AedstCompilerWarnings.cmake`
  builds clean; CI uses warnings-as-errors. A suppression needs an inline comment
  with the reason and a link to the issue — never a blanket `-Wno-...`.
- Formatting: `.clang-format` (CI checks `--dry-run --Werror`). Static analysis:
  `.clang-tidy`; the blocking gate and its baseline are described in
  [CLANG_TIDY_BASELINE.md](CLANG_TIDY_BASELINE.md).
- Secrets: `aedst::core::SecretString` / `secureWipe`; never log them.

## Qt

- Ownership: QObject parent–child or a documented owner (`AppDependencies`);
  `QPointer` guards for callbacks that can outlive a QObject.
- Communication via signals/slots; no `QThread` subclassing, no polling loops.
- Business logic lives in Domain (Qt-free `Aedst::Core`); ViewModels adapt it to
  QML; QML files are stateless views. No `QQuickItem` in C++ logic.
- Long or blocking work goes off the GUI thread with an explicit lifetime and
  cancellation; never touch QObjects/QML from a worker thread.
- QML: every interactive component sets `Accessible.role/name`, has a visible
  focus ring (`Theme.ring`) and keyboard activation; user text uses `qsTr`; layouts
  (not fixed `x/y`) so windows resize and RTL mirrors. `qmllint` is blocking in CI.
- Production QML uses `qt_add_qml_module` (ahead-of-time compiled), never plain
  `qt_add_resources`.
- Platform-specific APIs live behind interfaces in `Aedst::Platform` /
  `Aedst::Persistence` with an availability check and a fallback.
- Design tokens come from `packages/design-tokens`; never edit `AedstTokens.qml`.
