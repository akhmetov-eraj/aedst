# Signing

Three independent signatures protect a desktop release. Only the first two are in place in code; the third needs a decision.

| What | Key | Where it lives | Verified by |
|---|---|---|---|
| Update manifest | Ed25519 | CI secret `AEDST_UPDATE_SIGNING_KEY`; public half compiled into the app (`AEDST_UPDATE_PUBLIC_KEYS`) | the app, before every update |
| APT / RPM repositories | OpenPGP | CI secrets `AEDST_GPG_PRIVATE_KEY`, `AEDST_GPG_PASSPHRASE`; public half at `/aedst-archive-keyring.{gpg,asc}` | apt / dnf on the user's machine |
| Windows binaries and installers | Authenticode certificate | **undecided** | Windows (SmartScreen, UAC, AV) |

## Update manifest key

```bash
openssl genpkey -algorithm ED25519 -out update-signing.pem        # keep offline; load into the CI secret
openssl pkey -in update-signing.pem -pubout -outform DER | tail -c 32 | xxd -p -c 64   # the 64 hex characters
```

Set the repository variable `AEDST_UPDATE_PUBLIC_KEYS` to `k1:<64 hex>`. Rotation: add the new key next to the old one
(`k1:...,k2:...`), ship a release, switch the CI secret to the new key and keyId, remove `k1` in a later release.
Losing the key means shipping a new build with a new key by hand (users must install it manually): back it up.

## Windows certificate - decision needed from the owner

Plan rule: the choice of signing certificate is not made by the implementer. The options, as they apply here:

1. **EV code-signing certificate on a hardware token or HSM**: immediate SmartScreen reputation; the token cannot be
   used on GitHub-hosted runners (needs a self-hosted runner with the token, or a cloud HSM that exposes a CSP).
2. **OV certificate (PFX in a secret)**: works with the existing `sign.ps1` and GitHub-hosted runners; SmartScreen reputation
   builds over time; since 2023 CAs issue OV keys on hardware too, so a plain PFX may not be obtainable.
3. **A managed signing service** (for example Azure Artifact/Trusted Signing): no key custody, works from hosted runners
   with OIDC; availability depends on the publisher's country/organisation type. Needs `signtool` with a dlib instead of a
   PFX, so `sign.ps1` and `vpk --signParams` change.
4. **Unsigned**: installers show SmartScreen warnings; updates still work (the manifest is signed), but not recommended.

Until chosen, CI signs only when `WINDOWS_CERT_PFX_BASE64` and `WINDOWS_CERT_PASSWORD` exist and otherwise warns.
The MSIX `Publisher` must equal the certificate subject exactly (`AEDST_MSIX_PUBLISHER`).

## Linux archive key

```bash
gpg --quick-generate-key "AEDST Package Signing <packages@aedst.com>" ed25519 sign 2y
gpg --armor --export-secret-keys <fingerprint>      # into the CI secret; then store the key offline
```
