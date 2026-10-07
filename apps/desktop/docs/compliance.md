# Licenses and compliance

Decision record: [ADR-0032](adr/0032-licenses-compliance.md). This page is the practical side.

## Qt: LGPL-3.0, dynamic linking, replaceable

The client uses Qt's official binaries under the **LGPL-3.0**, never commercial. What that requires, and how it is met:

| Obligation | How |
|---|---|
| Link dynamically so the user can replace Qt | Qt ships as shared libraries next to the executable (`AEDST_DEPLOY_QT`; Flatpak uses the KDE runtime). Qt is never linked statically. |
| License text and notices with the product | `compliance/licenses/LGPL-3.0.txt` and `GPL-3.0.txt` are installed into `share/doc/.../licenses`; the generated `THIRD_PARTY_NOTICES.txt` carries both texts, the Qt/FFmpeg attribution and the source offer. |
| No terms that forbid modification for own use or reverse engineering to debug such modifications | The installers' license pages (NSIS/MSI) must not add such terms. Review before writing an EULA. |
| Source for the Qt version used | The version is pinned in `qt-version.txt`; the notice points to download.qt.io and offers the source on request. |

**GPL-only Qt modules** (Qt Network Authorization, Qt GRPC, Qt Graphs, Qt Quick 3D, ... the list in `compliance/policy.json`,
taken from doc.qt.io/qt-6/licensing.html on 2026-10-08) cannot be shipped in a closed-source LGPL product.
`scripts/compliance/compliance.mjs qt` (lint job) fails when one is used without a waiver.

### Open decision: QtNetworkAuth

`Aedst::Oidc` (ADR-0025) uses **QtNetworkAuth, which Qt offers only under GPLv3 or a commercial license**. It is currently a
documented waiver in `compliance/policy.json`, not a resolved issue. Options for the owner:

1. Buy a commercial Qt license (then every Qt module is allowed and the LGPL machinery above becomes optional).
2. Replace QtNetworkAuth with a Qt Network based Authorization Code + PKCE flow (a few hundred lines: loopback listener,
   PKCE/state, token exchange, refresh; `OidcSession`, the mock provider and its tests stay). This keeps LGPL.
3. Ship the client under GPLv3 (not compatible with a closed product).

Nothing was decided or bought. **Do not release the OIDC build until this is settled.**

## FFmpeg (Qt Multimedia backend)

Qt Multimedia's FFmpeg backend loads FFmpeg libraries. They must be an **LGPL** build: `--enable-gpl`, `--enable-nonfree` and
GPL codecs (x264, x265, fdk-aac) would make the product GPL or non-redistributable. `compliance.mjs ffmpeg --dir <Qt dir>`
reads the build configuration string embedded in each `avcodec`/`avformat`/... library and fails on such flags; CI runs it
against the Qt installation the package is built with, so a changed Qt build is caught. A related legal question that is not a
copyright license: **codec patents** (H.264/HEVC/AAC decoding). Ask the business owner whether a patent pool license is
needed for the markets served.

## Third-party notices and SBOM

`compliance.mjs notices` (package job, `--strict`) merges the per-port `vcpkg.spdx.json` files of the vcpkg install with
`compliance/components.json` (Qt, qtkeychain, Velopack, FFmpeg: things vcpkg does not install) into:

- `THIRD_PARTY_NOTICES.txt`: every component with license, homepage and copyright text (vcpkg `copyright` files, FetchContent
  license files) plus the LGPL/GPL texts; installed into the package;
- `sbom.spdx.json`: one SPDX 2.3 document (root `aedst-desktop` DEPENDS_ON every component), uploaded as the
  `compliance-<OS>` artifact and attached to the release bundle by `desktop-release.yml`.

CycloneDX is not produced separately: SPDX is what vcpkg emits natively, and a converter would be a new tool for no extra
information. If a customer requires CycloneDX, add `cyclonedx-cli convert` as a release step.

Strict mode fails on: an undeclared license (`NOASSERTION`), a denied one (`GPL-*`, `AGPL-*`, ...), one that is not on the
allow list (review it, then add it to `compliance/policy.json` in the same PR), or an LGPL library marked static in
`components.json`.

## Adding a dependency

vcpkg port: nothing to do unless the check fails. Anything else (FetchContent, prebuilt SDK): add it to
`compliance/components.json` with its license and `licenseFile`, and to `docs/content/library-first.md`.
