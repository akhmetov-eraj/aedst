# Desktop packaging

All artifacts land in `build/<release-preset>/packages/`; debug symbols in
`build/<release-preset>/symbols/`. CI (`.github/workflows/desktop-ci.yml`, job `package`)
builds them, `install-verify` installs/uninstalls them on a clean runner.

Nothing here has been run in the authoring environment (no toolchain): the first CI run is
the acceptance test.

## Artifacts

| Platform | Preset | Command | Output |
|---|---|---|---|
| Windows | `msvc-release` | `cpack -G "NSIS;ZIP"` | `aedst-desktop-<v>-Windows.exe` (NSIS), `.zip` |
| Windows | `msvc-release` | `--target package_nsis` | hand-written NSIS (`windows/installer.nsi.in`), needs `makensis` |
| Windows | `msvc-release` | `--target package_msix` / `sign_msix` | `.msix` (unsigned unless `-DAEDST_MSIX_SIGN=ON` + certificate) |
| Linux | `gcc-release` | `cpack -G "DEB;RPM;TGZ"` | `.deb`, `.rpm`, `.tar.gz` (prefix `/usr`) |
| Linux | `gcc-release` | `--target package_appimage` | `.AppImage`; needs linuxdeploy + a real icon; not in CI |
| Linux | `gcc-release` | `--target package_flatpak` | `.flatpak`; needs flatpak-builder; not in CI |
| macOS | `macos-release` (arm64), `macos-x64-release` | `cpack -G "DragNDrop;productbuild"` | `.dmg`, `.pkg` (`-macOS-<arch>`) |

Universal (arm64 + x86_64) macOS binaries are not automated: vcpkg builds one triplet per
tree; build both presets and merge with `lipo -create`.

## Signing

Unsigned builds work everywhere (CI on forks, local). Signing is enabled purely by secrets;
each OS job computes a `HAS_SIGNING_SECRETS` flag from the secrets being present.

| Secret | Used for |
|---|---|
| `WINDOWS_CERT_PFX_BASE64`, `WINDOWS_CERT_PASSWORD` | `packaging/windows/sign.ps1` (signtool, SHA-256, RFC 3161 timestamp): exe before packaging, installer after |
| `APPLE_CERT_P12_BASE64`, `APPLE_CERT_PASSWORD` | one .p12 with both Developer ID Application and Developer ID Installer certificates |
| `APPLE_SIGNING_IDENTITY`, `APPLE_INSTALLER_IDENTITY` | identity names (`Developer ID Application: ...`, `Developer ID Installer: ...`) |
| `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_SPECIFIC_PASSWORD` | `xcrun notarytool submit --wait` + `xcrun stapler staple` |
| `GPG_PRIVATE_KEY` (+ optional `GPG_PASSPHRASE`) | detached `.asc` signatures for `.deb`, `.rpm`, `.tar.gz` |

macOS flow: `cmake/packaging/MacOSPreBuild.cmake` signs the staged `.app` inside CPack
(hardened runtime + `macos/entitlements.plist`) when `APPLE_SIGNING_IDENTITY` is set;
`CPACK_PRODUCTBUILD_IDENTITY_NAME` (from `APPLE_INSTALLER_IDENTITY`) signs the `.pkg`; the workflow
then signs and notarizes the DMG and notarizes the PKG via `macos/sign-and-notarize.sh`
(`sign-app | sign-file | sign-pkg | notarize | verify`). The script can also authenticate with an
App Store Connect API key (`APPLE_API_KEY_PATH/ID`, `APPLE_API_ISSUER`) or a stored
`APPLE_NOTARY_PROFILE` instead of the Apple ID.

The Windows uninstaller itself (`Uninstall.exe`) is not separately signed.

## Verification

- `scripts/verify-install-windows.ps1`: silent install (`/S /D=`), `--version`, silent uninstall
  (`_?=` so it is synchronous), asserts app files and Start Menu entries are gone and user data is kept.
- `scripts/verify-install-linux.sh`: `apt-get install ./x.deb`, `--version`, `apt-get purge`, asserts every
  file the package owned is gone.
- `scripts/verify-install-macos.sh`: DMG (mount, copy, run) and PKG (install, run, remove);
  `AEDST_EXPECT_SIGNED=1` also requires `codesign --verify` and `spctl --assess`.

All of them require `aedst-desktop --version` to print `AEDST_VERSION` to stdout and exit 0
without a display (App/src/main.cpp).

Manual signature checks: Windows `signtool verify /pa <file>`; macOS `spctl --assess --type
install|execute -v <file>` and `xcrun stapler validate <file>`; Linux `gpg --verify x.deb.asc x.deb`.

## Uninstall behaviour

- NSIS: the uninstaller removes the install directory, Start Menu folder and the Add/Remove
  Programs entry. A prompt offers to delete user data (`%APPDATA%\AEDST`, `%LOCALAPPDATA%\AEDST`);
  the default is No, and silent uninstalls always keep user data. The paths must match the
  organization/application names set in `main.cpp`.
- DEB/RPM: the package manager removes what the package owns; user data under `~` is never touched.
- macOS: drag the app to the Trash (DMG) or delete `/Applications/aedst-desktop.app` (PKG has no uninstaller).

## Symbols

`aedst_split_debug_symbols` (Release only): MSVC `/Z7` + `/DEBUG` -> `aedst-desktop.pdb`;
GCC/Clang `-g1` + `--build-id` -> `aedst-desktop.debug`; macOS `dsymutil` -> `.dSYM`. CI uploads them as
`aedst-symbols-<os>` (90 days), separate from the packages.

## Flatpak

`finish-args` grant `org.freedesktop.secrets` (Secret Service), `org.freedesktop.Notifications` and
`org.freedesktop.portal.Desktop` (autostart via the Background portal). UNVERIFIED: that the KDE runtime
ships libsecret at the version `AedstPlatform` expects.
