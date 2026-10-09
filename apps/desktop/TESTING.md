# Testing — AEDST Desktop

> Nothing below has executed yet: the tree has never been compiled. Expect the
> first CI run to surface failures in tests as well as in code; fix the cause,
> never weaken the assertion.

## Layers and labels

| Requirement | Where | Framework | CTest label |
|---|---|---|---|
| Unit (Domain, use cases, validators, pure logic) | `Modules/**/tests/*_test.cpp` | GoogleTest/gMock | `unit` |
| Integration: API | `Network/tests/connect_client_test.cpp` (loopback `FakeHttpServer`, real `QNetworkAccessManager`, real generated messages), `Auth/tests/connect_auth_remote_data_source_test.cpp` | GoogleTest | `unit` |
| Integration: SQLite / repositories / secrets | `Persistence/tests/` (`migrator`, `token_store`, `secure_storage`, `master_key_provider`) | GoogleTest | `unit` |
| QObject / ViewModels / platform glue | `Auth/tests/login_view_model_test.cpp`, `Connectivity`, `DesignSystem/tests`, `Platform/tests` (single instance) | Qt Test | `qttest` |
| UI (main user flows) | `Auth/tests/qml/tst_login_view.qml` (fake view model), `Admin/tests/qml/tst_admin_view.qml` (real view model, scripted repository), `App/tests/qml/tst_main_shell.qml` (the shell at 480 x 480 and wide, error banner, offline footer), `DesignSystem/tests/qml/tst_designsystem.qml` (every `Aedst*` component instantiates + accessibility/keyboard/narrow-window behaviour). Views of Tasks, Search, Notifications and Media still have no QML test | Qt Quick Test | `qmltest` |
| TLS | `Network/tests/connect_client_tls_test.cpp`: a real handshake against `FakeHttpServer`'s HTTPS mode (private test CA in `Testing/.../test_tls_material.hpp`): untrusted server refused and not retried, extra CA, SPKI pin match / mismatch | GoogleTest | `unit` |
| Performance | `Network/bench` (QBENCHMARK, `bench` preset); `PerformanceBudget` test in Persistence | Qt Test / GoogleTest | `bench` / `unit` |
| Security | secrets (tamper/wrong-key/legacy-migration), transport policy table, redaction, deep-link allow-list, update signature/rollback/hash, crash-upload consent | GoogleTest | `unit` |
| Regression | every fixed bug gets a test named after the failure (`TEST(Area, Regression_<what>)`) in the module it lives in | any | as above |
| Cross-platform | CI matrix: Windows (MSVC), Linux (GCC, Clang) | — | all labels |

The crash-signal handlers and Windows minidump filter are process-fatal code and
are **manually verified** (checklist in `Modules/Core/CrashReporting/CMakeLists.txt`);
that limitation is intentional and stated, not hidden.

## Running

```bash
cmake --preset gcc-debug && cmake --build --preset gcc-debug
ctest --preset gcc-debug                    # everything, offscreen platform
ctest --preset gcc-debug -L unit            # or qttest / qmltest
```

Sanitizers (`AEDST_SANITIZER`): presets `gcc-asan`, `clang-asan`, `clang-tsan`,
`msvc-asan`; the test presets export `ASAN_OPTIONS`, `UBSAN_OPTIONS`,
`TSAN_OPTIONS` and suppression files from `cmake/sanitizers/`. Suppressions are
empty until a real run justifies one — each entry needs a comment and an issue.

Coverage: preset `coverage` + target `coverage` (gcovr) — HTML/XML artifact in CI.

TODO (needs the first successful coverage run): add `--fail-under-line <N>` to the `gcovr` call in `Tests/CMakeLists.txt`, with N set a few
points below the measured line coverage and raised as it grows. A threshold chosen before any measurement would be either
meaningless or permanently red, so none is set.

## Rules

- No sleeping for asynchronous results: use `QSignalSpy::wait` with a bounded
  timeout or an injected clock (`Aedst::Testing` `FakeClock`).
- Tests must not touch the user's real keyring, data directory or network:
  use temp dirs (`TempDirectory`), `InMemoryMasterKeyProvider`, `FakeHttpServer`.
- New public behaviour needs a test in the same change; new bug fixes need a
  regression test that fails without the fix.
- UI tests run with `QT_QPA_PLATFORM=offscreen` and `QT_QUICK_CONTROLS_STYLE=Basic`.
