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

## Signing

Unsigned builds work everywhere (CI on forks, local). Signing is enabled purely by secrets;
each OS job computes a `HAS_SIGNING_SECRETS` flag from the secrets being present.

| Secret | Used for |
|---|---|
| `WINDOWS_CERT_PFX_BASE64`, `WINDOWS_CERT_PASSWORD` | `packaging/windows/sign.ps1` (signtool, SHA-256, RFC 3161 timestamp): exe before packaging, installer after |
| `GPG_PRIVATE_KEY` (+ optional `GPG_PASSPHRASE`) | detached `.asc` signatures for `.deb`, `.rpm`, `.tar.gz` |

The Windows uninstaller itself (`Uninstall.exe`) is not separately signed.

## Verification

- `scripts/verify-install-windows.ps1`: silent install (`/S /D=`), `--version`, silent uninstall
  (`_?=` so it is synchronous), asserts app files, Start Menu entries, the `aedst://` registration and the per-user
  fallback shortcut are gone and user data is kept; also checks the PE hardening flags and the protocol registration.
- `scripts/verify-install-linux.sh`: `apt-get install ./x.deb`, no Qt files outside `<libdir>/aedst-desktop`,
  hardening (readelf: PIE, RELRO, BIND_NOW, non-exec stack, stack protector, RUNPATH), `--version`, `apt-get purge`,
  asserts every file the package owned is gone.

All of them require `aedst-desktop --version` to print `AEDST_VERSION` to stdout and exit 0
without a display (App/src/main.cpp).

Manual signature checks: Windows `signtool verify /pa <file>`; Linux
`gpg --verify x.deb.asc x.deb`.

## Uninstall behaviour

- NSIS: the uninstaller removes the install directory, Start Menu folder and the Add/Remove
  Programs entry. A prompt offers to delete user data (`%APPDATA%\AEDST`, `%LOCALAPPDATA%\AEDST`);
  the default is No, and silent uninstalls always keep user data. The paths must match the
  organization/application names set in `main.cpp`.
- DEB/RPM: the package manager removes what the package owns; user data under `~` is never touched.

## Symbols

`aedst_split_debug_symbols` (Release only): MSVC `/Z7` + `/DEBUG` -> `aedst-desktop.pdb`;
GCC/Clang `-g1` + `--build-id` -> `aedst-desktop.debug`. CI uploads them as
`aedst-symbols-<os>` (90 days), separate from the packages.

## Flatpak

The manifest is `linux/flatpak/com.aedst.Desktop.yml`; its `finish-args` are the whole permission set and are kept
minimal on purpose:

| Permission | Why |
|---|---|
| `--share=network` | API, OIDC (including the loopback redirect listener), update checks |
| `--share=ipc` | X11 shared memory for Qt on X11 (harmless on Wayland) |
| `--socket=wayland`, `--socket=fallback-x11` | display; X11 only when there is no Wayland |
| `--device=dri` | GPU rendering for Qt Quick |
| `--socket=pulseaudio` | audio of `Aedst.Media` (PipeWire serves the PulseAudio socket) |
| `--talk-name=org.freedesktop.secrets` | the refresh token in the Secret Service (QtKeychain, libsecret) |
| `--talk-name=org.kde.StatusNotifierWatcher` | the tray icon (without it the icon is silently missing) |

Not requested, because every sandbox already reaches them: `org.freedesktop.Notifications` (native notifications) and
the `org.freedesktop.portal.*` portals (OpenURI for the OIDC browser, Background for autostart, FileChooser). No
`--filesystem=` access exists at all: files are picked through the portal. The Flatpak build uses
`-DAEDST_DEPLOY_QT=OFF -DAEDST_SYSTEM_QTKEYCHAIN=ON` (Qt comes from `org.kde.Platform`; the sandbox has no network at build
time). UNVERIFIED: that the KDE runtime ships libsecret at the version `AedstPersistence` (`libsecret_backend.cpp`) expects.

## Linux package layout and URL scheme

The DEB/RPM install under `/usr`. The bundled Qt is NOT in `/usr/lib`: it lives in the private directory
`<libdir>/aedst-desktop/` (libraries, `plugins/`, `qml/`, `translations/`), the executable finds it through a relative
RUNPATH and a `qt.conf` next to it (`cmake/AedstLinuxBundle.cmake`), and `scripts/verify-install-linux.sh` fails if any
`libQt6*` / `plugins` / `qml` file lands outside it. For RPM, the private libraries are neither provided nor required
(`CPACK_RPM_PACKAGE_AUTOPROV no`, `__requires_exclude`). Both `.desktop` files declare
`MimeType=x-scheme-handler/aedst` and `Exec=aedst-desktop %u`, so `aedst://` links reach the running instance
(`SingleInstanceGuard`); the Windows installers register `HKLMSoftwareClassesaedst` (removed on uninstall) and the MSIX
manifest declares the protocol.

## Windows Start Menu shortcut and the AppUserModelID

Native toasts need a Start Menu shortcut that carries the AppUserModelID `com.aedst.Desktop`. The installer creates the
shortcut and (when the NSIS `ApplicationID` plug-in is installed on the build machine) stamps the ID on it; the app only
VERIFIES it at start. If no shortcut carries the ID, the app writes a per-user fallback `ProgramsAEDST.lnk`, which the
uninstaller removes again. UNVERIFIED: none of this has run on Windows.
