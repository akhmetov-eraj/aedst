# ADR-0032: Licenses, LGPL compliance, FFmpeg and SBOM

- Status: accepted for the tooling; **one license decision is open** (QtNetworkAuth). Not run in the authoring environment.
- Scope: `apps/desktop` (`compliance/`, `scripts/compliance/`, `cmake/AedstPackaging.cmake`, CI)

## Context

Phase 11: LGPL dynamic linking and replaceability, license of the FFmpeg backend, automatic THIRD_PARTY_NOTICES from
vcpkg/SBOM, CycloneDX/SPDX SBOM in releases. Plan rule 7 forbids deciding a commercial Qt license on the owner's behalf.

## Findings

- Qt is dynamically linked everywhere (official shared binaries; Flatpak runtime), so LGPL-3.0 replaceability holds.
- **QtNetworkAuth is GPLv3/commercial only** (doc.qt.io: "Qt Network Authorization is available under commercial licenses from
  The Qt Company. In addition, it is available under the GNU General Public License, version 3."). Phase 4 (ADR-0025) chose it
  under the library-first rule without checking its license; that was a gap in this project's process, found now. All other Qt
  modules in use (Core, Gui, Qml, Quick, QuickControls2, Network, Widgets, Multimedia, DBus, Test) are LGPL-capable. Qt GRPC
  (also GPL-only) was already rejected in the Qt GRPC memo for other reasons.

## Decisions

1. **Tooling is code, not prose**: `scripts/compliance/compliance.mjs` (Node, standard library) with three checks: Qt module
   licenses (lint job), FFmpeg build flags and notices + SPDX SBOM (package job, strict). Policy in `compliance/policy.json`,
   extra components in `compliance/components.json`.
2. **SPDX, not CycloneDX**: vcpkg generates SPDX per port; merging is a small script, a converter would be a new tool.
3. **Legal files ship**: LGPL/GPL texts always, notices + SBOM when CI generated them; the release bundle carries both.
4. **QtNetworkAuth stays as an explicit, loud waiver** (printed on every run of the lint job) until the owner chooses: a
   commercial Qt license, or replacing it with a Qt Network PKCE flow (recommended if no commercial license is wanted; it keeps
   the LGPL position and the Phase 4 tests). The implementer did not buy, assume or replace anything.

## Unverified

- The vcpkg per-port SBOM location (`share/<port>/vcpkg.spdx.json`) and its license fields; the FetchContent license file paths;
  the FFmpeg library names in the Qt installation; reading `--enable-*` flags from the binaries. The first CI run decides.
- Legal sufficiency: this is engineering compliance, not legal advice; have the notices and the installer EULA reviewed.

## Revisit when

- The QtNetworkAuth decision is made (delete the waiver, update ADR-0025 and `library-first.md`).
- A new Qt module or a non-vcpkg dependency is added; Qt is upgraded (re-check the GPL-only list against the licensing page).
