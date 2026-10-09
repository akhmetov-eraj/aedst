# ADR-0033: Feature plugin architecture (static built-ins, optional signed run-time plugins)

- Status: accepted for the contract and the trust policy; **not compiled or run** (no Qt/CMake toolchain in the authoring environment).
- Scope: `apps/desktop/Modules/Core/Plugin` (+ `loader/`), `Modules/Features/*/Navigation`, `App/src/plugin_host.*`, `scripts/check-feature-boundaries.sh`

## Context

Features (Auth, Tasks, Search, Notifications, Media, Admin) were wired into `AppDependencies` by hand, with route
registration as six unrelated function calls. The server is adding `aedst.capabilities.v1`, so the client needs one place that
decides which features are available for this deployment and user, and a defined, safe seam for features that are not part of
the executable. Library-first: the loading mechanism is Qt's own `QPluginLoader`; nothing here re-implements plugin discovery,
signature verification (reuses `Aedst::Update`: `verifyEd25519`, `embeddedTrustedKeys`) or semver (reuses `parseSemVer`).

## Decisions

1. **One contract, `plugin::IFeaturePlugin`** (Qt-free, `Aedst::Plugin`): `descriptor()` (id, semver, `apiVersion`, required
   capabilities, feature flag), `routes()` (route name -> `qrc:/` view), `registerServices(ServiceRegistry&)`. `register` is a C++
   keyword, hence the name. `kPluginApiVersion` is checked exactly; the Qt IID carries the same version.
2. **Built-ins are static plugins.** Each feature exposes `make<Name>Plugin()` (replacing `register<Name>Destinations`); App adds
   them first through `PluginHost`. Built-ins and run-time plugins go through the same validation in `PluginManager`.
3. **Gating by capabilities.** `CapabilitySet` is a Qt-free value fed by an `ICapabilitySource`; the aedst.capabilities.v1 client
   is a later adapter, so nothing depends on generated code. An *unknown* set (not fetched, or the server predates the service)
   keeps built-ins active (first-party features must work offline; the server authorises every RPC anyway) and leaves run-time
   plugins pending (fail closed). Unmet requirements leave a plugin pending and `activate()` re-evaluates it later.
4. **Transactional registration.** `registerServices` runs on a snapshot of the `ServiceRegistry`; failure or an exception rolls it
   back and rejects the plugin. A service type can be provided once, so a plugin cannot replace a host service. Routes are unique
   across plugins and built-ins are added first, so a run-time plugin cannot shadow `auth/login`.
5. **Run-time plugins are opt-in at build time** (`-DAEDST_ENABLE_RUNTIME_PLUGINS=ON`, default OFF): the default executable has no
   code path that loads a library. When enabled, the only directory is `<application dir>/plugins`, fixed by `App/`.
6. **Trust policy (`plugin_trust.hpp`, pure and unit-tested; I/O in `PluginLoader`)**:
   - a file loads only if it is listed in `plugins.allowlist.json` with equal SHA-256 and size, and the allowlist's detached Ed25519
     signature (`plugins.allowlist.sig`, 64 raw bytes) verifies against a release key compiled into the binary
     (`AEDST_UPDATE_PUBLIC_KEYS`, the same keys that sign update manifests; rotation works the same way);
   - the allowlist is parsed only after its signature verified, and is all-or-nothing;
   - a present-but-invalidly-signed allowlist rejects everything in every build; **no allowlist means nothing loads in a release
     build**; the only unsigned path is a Debug (no `NDEBUG`) build with `AEDST_ALLOW_UNSIGNED_PLUGINS=1`, which a release build
     never reads;
   - release builds also refuse a plugin directory the current user can write to, skip symbolic links, cap file count and size,
     and check that the library's metadata id equals the allowlisted id and the created plugin's `descriptor().id`;
   - routes of any plugin must be `qrc:/` (no remote or file QML).
7. **Boundary rules**: `check-feature-boundaries.sh` additionally fails if a Core module (including Plugin) references a feature
   target, or if a feature links the run-time loader. Features still link Core only and never each other.

## Security notes (what this does and does not give)

- A loaded plugin is **native code in the app process**: there is no sandbox. The mitigation is provenance (signed allowlist,
  hash-pinned files, protected directory), not isolation. Anything that needs isolation belongs in a separate process (not built).
- A plugin gets only what the host puts in the `ServiceRegistry`; by default that is nothing. Do not put tokens, the token store
  or the database in it; hand out narrow interfaces (for example a `ConnectClient` bound to the plugin's own RPCs).
- Residual risk: time-of-check/time-of-use between hashing and `dlopen`/`LoadLibrary` if the directory is attacker-writable. In
  release this is closed by refusing writable directories; an installer must create `plugins/` with admin-only write.
- `QPluginLoader::unload()` is never called, so plugin code is never unmapped under live objects.
- Signing the allowlist and the plugin build/release process are owner/ops work (key custody is shared with `docs/signing.md`).

## Phase 2 (not done here)

Built-ins currently contribute identity, gating and routes only; their view models are still created in `AppDependencies`. Moving
view-model construction into `registerServices()` needs the host services (client, error centre, analytics) in the registry and a
QML-singleton registration story per plugin; it is a larger, riskier change to do uncompiled.

## Unverified

Everything: no compiler ran. Qt specifics to confirm on the first build: `Q_DECLARE_INTERFACE`/`qobject_cast` on a non-QObject
interface, `QPluginLoader::metaData()` not executing plugin code before the hash check passed (the check precedes it either
way), `QFileInfo::isWritable()` semantics for directories on Windows ACLs, and the capability names the server will publish.

## Revisit when

The capabilities service ships (replace the unknown source, move built-in requirements out of "none"); a real third-party plugin is
planned (then add process isolation or revoke the idea); Phase 2 starts; Qt adds a signed-plugin facility.
