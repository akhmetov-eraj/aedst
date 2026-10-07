# ADR-0030: Observability - crash reports, breadcrumbs, scrubbing, analytics

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Aedst::Telemetry`, `Aedst::Core` redaction, `App/`), `.github/workflows/desktop-ci.yml`

## Context

Phase 9 asked for: sentry-native symbol upload with sentry-cli; release and environment equal to the package version;
breadcrumbs from Qt logging; PII scrubbing; PostHog with the same consent mechanism as the web app, batched and
non-blocking. Most of it already existed (Sentry facade, opt-in consent store, batching PostHog client); this phase closes
the gaps and writes the privacy contract down.

## What leaves the machine, and when

| Channel | Content | Gate |
|---|---|---|
| Sentry crash report (crashpad, out of process) | minidump (thread stacks and some heap), module list, OS version, release, environment, breadcrumbs, opaque user id | crash-report consent (default **off**, `require_user_consent`); nothing is uploaded before "yes", revoking drops queued reports |
| Sentry handled error (`captureError`) | context label, scrubbed message, breadcrumbs | same consent |
| PostHog | event name, small static property vocabulary, `app_version`, OS family/version, anonymous per-install id (opaque user id after sign-in) | analytics consent (default **off**), separate from crash consent; withdrawal clears the queue and the cached flags |
| Support log file | redacted spdlog output | never uploaded by the app; the user attaches it |

Never sent: e-mail, name, IP address (the client adds none; enable "discard client IP" in the PostHog project), device or
machine name, tokens, request bodies. `TelemetrySettings` and the privacy dialog (`Main.qml`) are the only consent UI.

## Decisions

1. **Consent compared with the web app.** PostHog uses the same model as web: opt-in, "unknown" counts as "no",
   withdrawal stops collection immediately, identity is the opaque user id only. There is no desktop equivalent of DNT/GPC
   (browser signals), so that part of the web rule has no counterpart. **Deliberate difference:** web leaves Sentry *error*
   reporting on without consent (operational data, `decisions.md`), desktop gates crash reports behind consent too, because a
   minidump contains process memory and cannot be scrubbed. The stricter rule stays.
2. **One scrubber.** `core::scrubPii` = `redactSecrets` (keys, bearer tokens, e-mails) + account names inside profile paths
   (`C:\Users\<name>\`, `/home/<name>/`) -> `<user>`. It is applied to: breadcrumbs when they are added (so crashpad's
   out-of-process reports contain only scrubbed text), handled-error messages, a `before_send` pass over message, exception
   values, breadcrumbs and `transaction` (which also drops `server_name` and any `user.ip_address/email/username`), and every
   PostHog property value (also cut to 200 characters). Out-of-process crash events do not pass through `before_send`; that is
   why the breadcrumbs are scrubbed at the source.
3. **Breadcrumbs from Qt logging.** The Qt log bridge (ADR-0024) turns warnings and errors from every category and *Info*
   from first-party `aedst.*` categories into breadcrumbs (sign-in, navigation, updates, connectivity - the trail before a
   crash). Debug never. The sink is injectable (`QtLogBridgeOptions::breadcrumbSink`) so it is tested without Sentry.
4. **Release and environment.** `release = "aedst-desktop@" + PROJECT_VERSION` (the string `--version` prints), the same
   value the CI uses for `sentry-cli releases new / debug-files upload / releases finalize`; the job fails if the built binary
   reports another version. `environment`: `production` (stable tag), `beta` (pre-release tag), `ci` (anything else). The
   symbol upload runs only in the release flavour (`inputs.release`), not for every push.
5. **PostHog delivery** stays as built: events queue in memory, flush every 30 s or at 20 events, one request in flight, 15 s
   timeout, re-queued on network failure (cap 500), dropped on 4xx, never blocking the GUI thread (Qt Network). Not persisted:
   events right before a crash are lost by design.

## Consequences

- No new dependency (`@sentry/cli` 3.8.0 was already used in CI via `npx`; verified to exist on npm).
- The sentry-native C API calls in `sentry_reporter.cpp` (`before_send`, value accessors) are compiled only with
  `AEDST_WITH_SENTRY` and were written from the documented API without a compiler: UNVERIFIED.

## Not done / revisit when

- Performance traces and PostHog session recording are not used (web uses Replay; desktop has no equivalent).
- Symbolication was not verified end to end (needs a real crash from a signed release build).
- Revisit the Sentry consent default if a legal review says crash data may be collected under legitimate interest.
