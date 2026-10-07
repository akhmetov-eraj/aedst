# ADR-0031: Performance gate and cancellable media work

- Status: accepted (not compiled or run in the authoring environment; the budgets are provisional)
- Scope: `apps/desktop` (`scripts/perf/`, `perf/`, `Modules/Core/Media/`, `Modules/Features/Auth/`, `.github/workflows/desktop-ci.yml`)

## Context

Phase 10 asked for automated budgets (cold start, memory, scrolling), QML profiling guidance (async images, Loader, C++
models), media work in worker threads with cancellation, and benchmarks with a regression threshold in CI.

## Audit result (what was already right)

- Collections are C++ `QAbstractListModel`s with named roles; no `QVariantList` models in the list screens.
- Images are `asynchronous: true` with a `sourceSize` (`MediaImage`, `AedstAvatar`, `VideoPlayer`); the viewer uses a `Loader`.
- Image decoding, compression and hashing already run on `QThreadPool`; downloads on the GUI thread's `QNetworkAccessManager`.
- All QML modules were ahead-of-time compiled **except the sign-in screen** (`LoginView.qml`, plain `qt_add_resources`), the first
  thing every launch renders.

## Decisions

1. **Sign-in view becomes a static QML module** (`AedstFeatureAuthViews`, URI `Aedst.Feature.Auth.Views`), like the other
   feature views; its qrc URL changes to `qrc:/qt/qml/Aedst/Feature/Auth/Views/LoginView.qml` (route constant and QML test
   updated). Expected effect: no parse of the largest first-screen file at startup. Not measured.
2. **Cancellation in the media pipeline.**
   - `MediaImageFetcher::fetch` returns a `Ticket`; `MediaImageResponse::cancel()` (called by the engine when a delegate is
     recycled or its source changes) aborts the transfer, and a response cancelled before or after the download skips the
     decode. `finished()` is still emitted exactly once, as the `QQuickImageResponse` contract requires. Without this, fast
     scrolling kept every scrolled-away tile downloading and decoding.
   - `MediaUploadController` mirrors its generation counter into a shared atomic, so a preparation superseded by cancel/next/
     destroy stops before the encode and before the hash, and removes its temp file.
3. **One command for budgets**: `scripts/perf/run-perf.mjs` (Node standard library, no new dependency; the repo already
   requires Node for the web app) with `perf/budgets.json` (absolute limits) and `perf/baseline.json` (per-platform
   micro-benchmark baselines, 30 % regression threshold). A new benchmark with no baseline is recorded and passes, so adding one
   never breaks CI on its own.
4. **Scroll benchmark** (`AedstMediaScrollBench`) drives the real `MediaGrid` with a synthetic model and an in-process async
   image provider and reports frame-time percentiles; it exits non-zero over a budget.
5. **`cacheBuffer`** two rows ahead on `MediaGrid`. **`reuseItems` is not enabled**: tiles carry image state and nothing can be
   verified without running; enable only with a benchmark showing a gain and a visual check for stale thumbnails.

Rejected: Google Benchmark (a third benchmark library, see `docs/performance.md`), a custom frame-timing overlay in the
product build, QML profiler runs in CI (needs a debug-enabled Release build, which is forbidden).

## Unverified / revisit when

- **Every number**: budgets are guesses until the first CI run; the table in `docs/performance.md` is empty on purpose.
- The perf job's Linux frame times come from software rendering; use them only as a regression signal.
- Compile and behaviour of the cancellation code and the QML module move were not exercised (no toolchain here).
- Revisit `reuseItems` and a C++ `QAbstractListModel` for the task list when the scroll numbers show a frame-time problem.
- Startup budget measures an empty profile with the login screen as first frame; a signed-in start (restore session, database
  worker) is a different path and is not covered.
