# Desktop performance baseline

Rules and switches, each tied to something measurable. Nothing here is
"speculative": items that could not be justified without numbers are listed under
[Deliberately not done](#deliberately-not-done) with the criteria that would
change that.

> Nothing in this repo has been compiled or benchmarked in the authoring
> environment (no toolchain). The numbers below are the *procedure*; fill in the
> first real run in the PR that introduces a change.

## Measuring

| What | How |
|---|---|
| Startup | `startup +N ms: <phase>` lines in `<AppData>/logs/aedst.log` (`App/src/startup_trace.hpp`): logger ready → dependencies ready → QML loaded → first frame. Quote before/after for any startup change; take the median of 10 cold launches. |
| Per-call hot path | `cmake --preset bench && cmake --build --preset bench && ctest --preset bench -V` runs Qt Test `QBENCHMARK`s (`Modules/Core/Network/bench`): protobuf (de)serialize of a JWT-sized `LoginResponse`, envelope encode/decode, error-body parse. Release build only. |
| Scene graph | `QSG_INFO=1` (backend, GL info), `QSG_VISUALIZE=overdraw`, Qt Quick Profiler / Creator's QML Profiler (needs a Debug/RelWithDebInfo kit with QML debugging enabled — never Release, see below). |

## Build-time switches (all in `cmake/`, applied in the root `CMakeLists.txt`)

| Switch | Default | Effect |
|---|---|---|
| Release optimisation | CMake defaults (`/O2 /Ob2`, `-O3`) | Kept. `-O2` vs `-O3` is a measurement question; change only with numbers. |
| `AEDST_ENABLE_LTO` | ON (Release only) | `INTERPROCEDURAL_OPTIMIZATION_RELEASE` (MSVC `/GL /LTCG`, GCC `-flto`, Clang ThinLTO) when `check_ipo_supported` passes. Debug keeps fast links. |
| `AEDST_FAST_LINKER` | ON | Linux GCC/Clang: `-fuse-ld=mold`, else `lld`, when installed. |
| Section GC | Release, GCC/Clang | `-ffunction-sections -fdata-sections -Wl,--gc-sections`. |
| `AEDST_USE_COMPILER_CACHE` | ON | `sccache` (MSVC/GCC/Clang) else `ccache` as launcher; MSVC debug info becomes `/Z7` so it is cacheable. The main CI-time and local rebuild win, notably for protoc-generated code. |
| Unity build | opt-in | `-DCMAKE_UNITY_BUILD=ON`. Off by default: unity TUs hide missing includes, which the module-boundary rule relies on the compiler to catch. |
| PCH | not enabled | Add `target_precompile_headers` per target only with a measured build-time gain. |
| `QT_QML_DEBUG` | forbidden in Release | Configure fails if it appears in `CMAKE_CXX_FLAGS*` of a Release/MinSizeRel build (Qt Creator kits can add it). |
| Compiler/standard gate | always | `cmake/AedstCompilerFeatures.cmake`: MSVC 19.37+, GCC 13+, Clang 17+ and a real `std::expected` compile check — fails at configure with a clear message. |

## Runtime rules

- **Compile QML ahead of time.** `qt_add_qml_module` runs `qmlcachegen` (bytecode)
  for its `QML_FILES`; `App` already uses it for `Main.qml`. Files added with plain
  `qt_add_resources` (`Modules/Core/DesignSystem`, `Modules/Features/Auth`'s
  `LoginView.qml`) are **parsed and compiled at every launch** — the largest
  remaining startup lever. Migration recipe (owned by the DesignSystem work, not
  done here to avoid colliding with the token-injection change):
  `qt_add_qml_module(AedstDesignSystem URI AedstDesignSystem VERSION 1.0 QML_FILES Theme.qml AedstButton.qml ...)`
  with `set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)`,
  then drop the hand-written `qmldir`. For a static library module also link the
  generated `<target>plugin` target (or `Q_IMPORT_QML_PLUGIN`).
- **Declarative type registration** instead of `qmlRegister*` at run time:
  `QML_ELEMENT` / `QML_SINGLETON` (with a `static create(QQmlEngine*, QJSEngine*)`
  returning the instance `AppDependencies` built) on the ViewModels, listed under
  `SOURCES` of a `qt_add_qml_module`. Lets `qmlcachegen`/`qmllint` see the types
  and removes registration code from `main()`.
- **RHI/scene graph:** keep Qt's defaults (Direct3D 11 on Windows, OpenGL on
  Linux; threaded render loop). Override only via
  `QSG_RHI_BACKEND` while diagnosing; do not hard-code a backend in code.
- **Models:** expose collections as `QAbstractItemModel`/`QAbstractListModel`
  subclasses with named roles and fine-grained `dataChanged`/`rowsInserted`
  signals — not `QVariantList`/`QVariantMap` and not JS arrays rebuilt in
  bindings. No allocation per delegate per frame: keep delegates declarative,
  move formatting into C++ roles, avoid `Component.onCompleted` heavy JS and
  binding chains that re-evaluate on every frame.
- **Network:** one long-lived `ConnectClient` (one `QNetworkAccessManager`:
  keep-alive and HTTP/2 multiplexing depend on reuse); `prewarm()` after the
  first screen is requested; binary Protobuf on the wire; HTTP-level gzip is
  automatic. Never construct a manager per request.
- **Logging:** `AedstLogger` (spdlog) at Info in Release; per-call debug lines are
  compiled in but filtered — do not build strings for log lines by hand.
- **Threads:** Qt Network needs none. Move CPU-heavy work (large protobuf
  decode, SQLite bulk writes) to `QtConcurrent`/`QThreadPool`, and post results
  back with queued connections; never block the GUI thread.

## Deliberately not done

| Idea | Why not (yet) | Adopt when |
|---|---|---|
| mimalloc / other allocator | Windows: replacing the CRT allocator process-wide interacts with Qt and vcpkg DLLs; Linux glibc malloc is adequate for a GUI client. Unmeasured. | A profile shows allocator time (>~5% of a hot path or startup) *and* an A/B run with `vcpkg install mimalloc` shows a gain. |
| PCH | See table. | Clean-build time is a measured problem after the compiler cache. |
| `QT_USE_QSTRINGBUILDER`, `QT_NO_CAST_FROM_ASCII` | Behaviour/compile-time changes across all existing code; cannot be validated without a compiler. | Add together in one change with a clean compile. |
| Protobuf lite runtime | Blocked by `validate.proto` (see ADR-0001). | Protovalidate import removed. |
| Google Benchmark | Would add a third test/bench library; Qt Test's `QBENCHMARK` already ships with the stack. | Micro-benchmark needs `DoNotOptimize`-level control that Qt Test cannot give. |
