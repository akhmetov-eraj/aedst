# ADR-0035: Capability negotiation

- Status: accepted; **not compiled or run** (no Qt/CMake toolchain in the authoring environment). The tests below are written, not executed.
- Scope: `Modules/Core/Capabilities` (new), `Modules/Core/Contract` (`kCapabilitiesGetCapabilities`), `Modules/Core/Persistence`
  (migration 3, `kv` table), `App/` (`app_dependencies_capabilities.cpp`, `client_build.hpp`, `SessionInfo::visibleFeatures`, `Main.qml`).
- Counterpart of the client half of [ADR-0009](/decisions/0009-api-compatibility-and-client-versioning) on the other platforms. Behaviour is
  pinned by the shared fixture `docs/contract/behavior/capabilities.json`; the wire contract is
  `apps/server/api/proto/aedst/capabilities/v1/capabilities.proto`.

## Decisions

1. **The RPC is in the method table.** `kCapabilitiesGetCapabilities` (`AuthRequirement::Public`, `Idempotent`) in `contract/methods.hpp`.
   The messages are the protoc-generated `GetCapabilitiesResponse`; nothing is mirrored by hand and the service uses the generated type
   directly (`ServerCapabilities`). `methods_test.cpp` checks the entry against the descriptors and that no proto RPC lacks one.
2. **Rules are a pure function** (`evaluateCapabilities`, `capability_rules.cpp`), identical to the fixture and the other platforms:
   a feature is shown iff the build contains it AND the server enables it; `enabled = false` always wins; `required_client_version` hides
   it on an older client (semver via `Aedst::Update`; an unparsable requirement, or an unknown client version with a requirement, hides it:
   fail closed); a feature the server does not list falls back to the default (built-in on, flagged off); server names the build does not
   contain are ignored. Dotted names (`media.uploads`) are sub-capabilities: present iff the server enables them, the client meets their
   requirement and their coarse parent is shown.
3. **The client build is declared by the composition root** (`App/src/client_build.hpp`): the six coarse ids, none flagged yet. Core
   knows no feature.
4. **`CapabilitiesService` (QObject, GUI thread)** fetches unauthenticated, applies the answer, caches the serialized response and
   announces `capabilitiesChanged`. Source order: fetched answer, else last known (cache), else defaults. A failed fetch never clears
   anything; a cache read that completes after a fetch is ignored; one fetch in flight at a time. Network and cache are ports
   (`CapabilitiesRemote`, `CapabilityCache`) so the tests use doubles; production uses `ConnectCapabilitiesRemote` and
   `SqliteCapabilityCache` (the database thread, never the GUI thread).
5. **Cache = a generic `kv` table** (migration 3 in the single migration list, `persistence/kv_store.hpp`): non-secret blobs only. The
   capabilities answer is stored as its serialized protobuf under `capabilities.v1`; an unreadable entry is ignored.
6. **It is the plugins' `ICapabilitySource`.** `current()` is deliberately UNKNOWN until a fetched or cached answer exists, so the
   PluginManager semantics of ADR-0033 are unchanged (built-ins work offline and against old servers, run-time plugins wait).
   `isEnabled()` applies the defaults and is what the shell uses. Each new answer re-runs `PluginHost::activate()` and registers
   the routes of plugins that became active.
7. **Shown = hidden in the shell.** `SessionInfo::visibleFeatures` (the `Session` singleton) feeds `Main.qml`: a section (Tasks, Search,
   Files, Notifications, Admin) and its shortcut exist only while its id is listed. The defaults are in effect before the first answer.
   `PluginManager` has no deactivation (ADR-0033), so a built-in plugin that the server disables stays registered in the NavGraph; it is
   unreachable from the chrome, not unloaded.
8. **Triggers.** `start()` after the first screen is on its way (cache, then fetch) and `refresh()` whenever `ConnectivityMonitor`
   reports online (both in `main.cpp`).
9. **Version window -> forced upgrade.** `adviseVersion` compares the build with `clients["desktop"]`: below `minimum` ->
   `upgradeRequired()` -> `AppDependencies::onUpgradeRequired()` -> `Router::lockTo(app/upgrade-required)` (the same path as the version
   gate's `CLIENT_UPGRADE_REQUIRED` refusal, idempotent). Below `recommended`, or `update_status = RECOMMENDED` ->
   `upgradeRecommended()` (non-blocking), currently logged only.

## Known gaps / revisit when

- Nothing was compiled or run. The first CI build is the acceptance test.
- The cached answer is read asynchronously, after plugin activation at startup, so a feature the server disabled is hidden in the shell as
  soon as the cache is read (a moment after launch), not before the first frame.
- `upgradeRecommended()` has no UI yet (a banner needs strings in en/ru/tg/uz/ar); it should offer `UpdateService::checkNow`.
- No desktop feature is flagged. When one is, add it to `ClientBuild::flagged` and gate its plugin with `PluginDescriptor::featureFlag`.
- A dedicated periodic refresh (the server says the answer is cacheable for a few minutes) is not implemented; launch and
  connectivity-restore only. Revisit if a session routinely outlives an operator's switch-off.
