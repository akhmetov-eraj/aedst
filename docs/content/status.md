# Platform status and profiles

What has actually been run, and which parts of the template a project needs. Read this before building on a platform: "Accepted" in [decisions.md](/decisions) records a *decision*, not that the code builds.

## Verification status

| Platform | Status | Evidence |
|---|---|---|
| Server | **Verified** | `go build ./...`, `go vet ./...` and `go test ./...` (unit tests) pass. Integration tests need Postgres (Docker or `AEDST_TEST_DATABASE_URL`); Helm/CloudNativePG/Argo CD manifests are **not verified against a cluster** ([deployment](/deployment#verification-status-read-this-before-trusting-the-manifests)) |
| Web | **Verified** | `tsc --noEmit`, Biome and the Vitest suite (371 passed, 35 skipped) pass; `next build` and Playwright were not run in this pass. Playwright runs against an in-memory mock of the server, not the real one |
| Android | **Not compiled** | Written without a JDK/Android SDK. First `make generate` + Gradle build is the acceptance test |
| Apple | **Not built** | Written without Xcode. First `tuist generate` + `xcodebuild` is the acceptance test; the release scripts have never run |
| Desktop | **Not built** | Written without a Qt/vcpkg toolchain. First CMake configure/compile is the acceptance test; the CI gates have never executed |

Expect the first real build of Android, Apple and Desktop to surface compile errors, version mismatches and CI gates that need tuning. Record each finding (and the fix) in [decisions.md](/decisions) instead of silently patching, and update this table when a platform passes its first build.

### Exit criteria for "Verified"

A platform moves to **Verified** only when all of these have run on a clean machine or CI runner and passed:

- **Android**: `task android:generate android:lint android:test android:build` (JDK 21).
- **Apple**: `task apple:generate apple:lint apple:test` on macOS with Xcode, plus a simulator launch.
- **Desktop**: `task desktop:build desktop:test` on one preset per OS (`msvc-*`, `gcc-*`, `macos-*`).

## Profiles

The template is deliberately complete; most products need less. Pick a profile and delete or ignore the rest — the server contract and the shared design tokens do not depend on which clients you keep.

| Profile | Keep | Drop |
|---|---|---|
| **Core** (default for a new product) | `apps/server`, `apps/web`, `docs`, `packages/design-tokens` | `apps/android`, `apps/apple`, `apps/desktop` |
| **Core + mobile** | Core + `apps/android` and/or `apps/apple` | `apps/desktop` |
| **Full** | everything | — |

Infrastructure scales the same way. Start with Docker Compose (`task dev`) and one Postgres. Move to the HA Postgres / Helm / GitOps setup in [deployment](/deployment) when you have a production target; none of it is needed to develop.

### Removing a platform

1. Delete `apps/<platform>` and its workflows in `.github/workflows` (`<platform>-*.yml`).
2. Remove its includes and `<platform>:*` tasks from `Taskfile.yml` and from the aggregate `generate`, `lint`, `test`, `build` and `check` tasks.
3. Remove its path from `check:boundaries`, `renovate.json`, `mise.toml` tools that only it needs (for example `java` for Android) and `scripts/update-deps.sh`.
4. Remove its rows from the "Approved stack" tables in [library-first](/library-first) and its section in [decisions.md](/decisions).
5. Keep the server `.proto` contract unchanged; it is shared by every client.

Run `task check` afterwards: naming, boundaries and tooling checks must still pass.
