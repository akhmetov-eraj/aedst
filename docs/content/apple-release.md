# Apple release, signing and App Store readiness

How the iOS and macOS apps get from `main` to TestFlight, the Mac App Store, or a notarized direct download. Everything here is **scripts + GitHub Actions on platform tools** (`xcodebuild`, `notarytool`, `stapler`); no fastlane.

> **Verification status.** The release scripts and `apple-release.yml` were written without an Xcode toolchain and **have never been run**. The first real run is their acceptance test; fix what it flags and update this page. CI (`apple-ci.yml`) is what proves the code compiles: its `app-build` job builds both app schemes.

## What is in the repo (so a release is only credentials away)

| Concern | Where |
|---|---|
| Hardened runtime (needed for notarization) | `Config/macOS.xcconfig` (`ENABLE_HARDENED_RUNTIME = YES`) |
| App Sandbox + network client entitlement | `App/macOS/AEDST.entitlements` |
| Release optimisation, dSYM, stripping | `Config/Release.xcconfig` |
| Signing (team id never committed) | `Config/Base.xcconfig` includes the gitignored `Config/Signing.local.xcconfig` (copy the `.example`), or `DEVELOPMENT_TEAM=` on the command line (CI) |
| Privacy manifests (tracking off; email, user id, device id collected for app functionality) | `App/iOS/PrivacyInfo.xcprivacy`, `App/macOS/PrivacyInfo.xcprivacy` |
| Face ID usage description, localized (en, ru, ar, tg, uz) | `App/iOS/Info.plist` + `App/iOS/Resources/*.lproj/InfoPlist.strings` |
| Export encryption compliance (`ITSAppUsesNonExemptEncryption = NO`: HTTPS only) | both `Info.plist` files |
| Export options (TestFlight, Developer ID, Mac App Store) | `ExportOptions/*.plist` (team id substituted at run time) |
| Archive / export / upload / notarize / staple | `scripts/release-ios.sh`, `scripts/release-macos.sh`, `make release-ios`, `make release-macos` |
| Gated CI release | `.github/workflows/apple-release.yml` (manual, `apple-release` environment) |
| Pre-release checks on every PR | `apple-ci.yml`: plist/privacy/entitlements lint, release scripts parse, unsigned Release build of both apps, privacy manifest present in the built `.app` |

## One-time setup (needs a paid Apple Developer Program account)

1. **App IDs**: register `com.aedst.apple.ios` and `com.aedst.apple.macos` (or change the prefix in `Project.swift`).
2. **App Store Connect API key** (Users and Access > Integrations > App Store Connect API, role *App Manager*): note the Key ID and Issuer ID, download the `.p8` once.
3. **GitHub Environment `apple-release`** with *required reviewers*, and these secrets:

   | Secret | Value |
   |---|---|
   | `APPLE_TEAM_ID` | 10-character team id |
   | `ASC_KEY_ID`, `ASC_ISSUER_ID` | from step 2 |
   | `ASC_KEY_P8_BASE64` | `base64 -i AuthKey_XXXX.p8` |
   | `DEVELOPER_ID_CERT_P12_BASE64`, `DEVELOPER_ID_CERT_PASSWORD` | *Developer ID Application* certificate exported as `.p12` (macOS direct distribution only) |

4. **App Store Connect records** for both apps (name, bundle id, privacy policy URL, age rating, screenshots, export compliance answers, App Privacy "nutrition label" matching the privacy manifests).
5. Locally: `cp Config/Signing.local.xcconfig.example Config/Signing.local.xcconfig` and put your team id in it.

## Releasing

| Goal | Command |
|---|---|
| iOS build to TestFlight | run **apple-release** with `ios-testflight`, or locally `UPLOAD=1 APPLE_TEAM_ID=... make release-ios` |
| macOS notarized download | **apple-release** with `macos-developer-id` (artifact `AEDST-*.zip`), or `make release-macos` |
| macOS Mac App Store | **apple-release** with `macos-app-store`, or `DISTRIBUTION=app-store UPLOAD=1 make release-macos` |

The build number is the GitHub run number (`CURRENT_PROJECT_VERSION`); the marketing version is `MARKETING_VERSION` in `Config/Base.xcconfig` (bump it by PR).

## Checklist before the first submission

- [ ] `make bootstrap` and a Debug run on a device and in the Simulator: login against a real server, kill and relaunch (session restored), airplane mode (offline banner, app still opens signed in), sign out.
- [ ] Xcode > Product > Archive > *Generate Privacy Report*: the aggregated report must match `PrivacyInfo.xcprivacy` and every third-party SDK's manifest (GRDB, swift-protobuf, connect-swift). If it lists a required-reason API app code uses, declare it.
- [ ] Accessibility Inspector pass with Dynamic Type at the largest sizes, VoiceOver, Reduce Motion, Increase Contrast, in light and dark.
- [ ] RTL check (Arabic): `-AppleLanguages (ar)` and `-NSDoubleLocalizedStrings YES` launch arguments.
- [ ] Verify on a device that `http://localhost` is only reachable in Debug: Release builds resolve to `production` (`APIEnvironment.resolve`, covered by tests) and the app declares no ATS exceptions.
- [ ] Confirm the Keychain accessibility choice still fits the product (no background token access is needed today).

## Deliberately not in the repo

- **Universal Links** (`https://` deep links): they need an `apple-app-site-association` file served by the backend/website and the *Associated Domains* entitlement on a signed team. The custom scheme `aedst://login` works without either.
- **Sign in with Apple / Passkeys**: they require server support (identity provider configuration, `apps/server` Keycloak/OIDC). App Store guideline 4.8 applies as soon as any third-party social login is added.
- **Push notifications** are in the repo: `aps-environment` is set from `$(APS_ENVIRONMENT)` (development in Debug, production in Release). Before release, enable Push Notifications on both App IDs, regenerate the provisioning profiles, and give the server an APNs `.p8` key (`AEDST_PUSH_APNS_*`). Release archives should run `apps/apple/scripts/upload-dsyms.sh <archive>` and pass `AEDST_SENTRY_DSN` / `AEDST_POSTHOG_KEY` as build settings.
- **Widgets, App Groups**: no product requirement yet; adding them means new entitlements and a new target each.
- **Keychain access group / shared session**: `Keychain(accessGroup:)` is ready, but sharing needs the `keychain-access-groups` entitlement and an extension target.
