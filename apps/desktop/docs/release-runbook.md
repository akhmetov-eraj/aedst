# Desktop release runbook

Background: [ADR-0029](adr/0029-delivery-updates-signing.md), keys: [signing.md](signing.md).

## One-time setup

Repository **variables**: `AEDST_UPDATE_PUBLIC_KEYS`, `AEDST_UPDATE_MANIFEST_URL` (for example
`https://updates.aedst.com/stable.json`), `AEDST_UPDATE_FEED_URL` (Velopack feed base URL), `AEDST_DOWNLOAD_BASE_URL`.
Repository **secrets**: `AEDST_UPDATE_SIGNING_KEY`, `AEDST_GPG_PRIVATE_KEY`, `AEDST_GPG_PASSPHRASE`, optionally
`WINDOWS_CERT_PFX_BASE64`, `WINDOWS_CERT_PASSWORD`, the Sentry and PostHog secrets used by desktop-ci.
A missing value never fails the release; the workflow annotates what was skipped.

The beta manifest URL is the same with `beta.json`; a beta build reads it by `AEDST_UPDATE_CHANNEL=beta` (environment) -
there is no UI switch yet.

## Releasing

1. Bump `VERSION` in `apps/desktop/CMakeLists.txt`; merge.
2. `git tag desktop-v1.6.0 && git push origin desktop-v1.6.0` (`desktop-v1.7.0-beta.1` goes to the beta channel).
3. The `desktop-release` workflow builds both platforms (release flavour), runs `vpk pack`, signs the manifest and builds the
   APT/RPM repositories. Download the artifact `desktop-release-bundle`.
4. Check `SHA256SUMS`, install the setup on a clean VM and the AppImage on a clean Linux machine.
5. Upload in this order: packages (`velopack-*`, repository trees) first, **manifest last** - the manifest is what makes a
   release visible. Each manifest's `publishedAt` is the signing time and must keep growing within a channel.

## Rolling back a bad release

Build the previous tag again (or reuse its `*-full.nupkg`) and sign a rollback manifest with the same channel:

```bash
scripts/release/sign-update-manifest.sh --rollback --version 1.5.2 --channel stable --min-supported 1.0.0 \
  --notes-url https://aedst.com/releases/1.5.2 --artifact windows:x86_64:<url>:<file> --artifact linux:x86_64:<url>:<file> \
  --key-id k1 --out stable.json
```

Clients on the bad version install it as a mandatory update. Clients that crash before they can fetch it start in safe mode
after three failed starts and fetch only the rollback; users who cannot start the app at all reinstall from the website.

## Local test of the update flow

Point a build at a local https server: `AEDST_UPDATE_MANIFEST_URL=https://localhost:8443/stable.json` (TLS is mandatory in
the fetcher) with a throw-away key pair compiled in via `-DAEDST_UPDATE_PUBLIC_KEYS=...`. Unit tests for every decision
(`update_pipeline_test`, `update_service_test`, `update_checker_test`, `startup_health_guard_test`) run without any network.
