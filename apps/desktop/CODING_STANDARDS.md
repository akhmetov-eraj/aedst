# Coding standards — C++ and Qt (AEDST Desktop)

Naming lives in the repo-wide [CONVENTIONS.md](../../CONVENTIONS.md) (files
`snake_case`, `_test.cpp`; enforced by `.clang-tidy` and `scripts/check-naming.sh`).
Dependency choice lives in [library-first.md](../../docs/content/library-first.md):
never write what the standard library, Qt, or an approved dependency already does.

## C++

- C++23, no compiler extensions. Prefer `std::expected` via `core::Outcome<T>`
  for expected failures; exceptions are not used for control flow.
### Allowed C++23 subset

The tree is compiled as C++23 with extensions off, but only what **all** CI compilers
accept is allowed: MSVC 19.37 (VS 2022 17.7), GCC 13 and Clang 17, the latter two with
libstdc++ 13. The table is from the compilers' conformance pages and is **UNVERIFIED**
until the first real CI run; correct it from the evidence.

| Feature | Status | Why |
|---|---|---|
| `std::expected` / `std::unexpected` (via `core::Outcome`, `ApiResult`) | **used** | GCC 12+, Clang 16+ with libstdc++, MSVC 19.33+; probed in `cmake/AedstCompilerFeatures.cmake` |
| C++20: `std::span`, `std::ranges` algorithms, designated initialisers, `std::type_identity`, `<=>`, `starts_with`/`ends_with` | **used** | supported everywhere |
| `std::to_underlying`, `std::unreachable`, `std::byteswap`, `if consteval`, `std::string::contains`, multidimensional `operator[]` | allowed, not used yet | supported by the three baselines |
| `std::move_only_function` | allowed with care | present in all three; confirm with libstdc++ 13 + Clang 17 in CI before relying on it |
| `std::print`/`println`, `std::flat_map`, `std::generator`, `std::mdspan`, `std::ranges::to`, `std::views::zip`/`enumerate`/`chunk`/`slide`/`join_with`/`cartesian_product`, `std::stacktrace`, `std::spanstream` | **banned** | missing in libstdc++ 13 (GCC 14/15) or unreliable with Clang 17 |
| Deducing `this`, `[[assume]]`, `import std;` / C++ modules | **banned** | GCC 14+/Clang 18+ only, or no build-system support |

Raising the baseline (for example GCC 14) is a deliberate change: update
`cmake/AedstCompilerFeatures.cmake`, the CI image, this table and
`scripts/check-cxx-subset.sh` together. The script runs in the CI `lint` job and fails on a
banned name; a documented exception needs a `cxx-subset: allow` comment on that line.

### General

- RAII everywhere; no owning raw pointers; `unique_ptr` by default,
  `shared_ptr` only where ownership is truly shared (DI graph).
- `const`, `[[nodiscard]]` on results that carry errors, `noexcept` where true.
- No blocking work on the GUI thread. Database access goes through `DatabaseWorker`
  ([ADR-0028](docs/adr/0028-local-data-database-thread.md)); network and keychain calls are asynchronous.
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
