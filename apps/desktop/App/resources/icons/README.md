# App icons (placeholder)

No binary assets are committed yet. Add real artwork here before shipping:

| File | Used by |
|---|---|
| `com.aedst.Desktop.png` (256x256) | AppImage (`packaging/linux/appimage/build-appimage.sh` fails without it), Flatpak/DEB icon theme install |
| `aedst.ico` (multi-size 16–256) | Windows executable resource + `CPACK_NSIS_MUI_ICON` in `cmake/packaging/Windows.cmake` |
| `Square44x44Logo.png`, `Square150x150Logo.png`, `StoreLogo.png` | MSIX `Assets\` (see `packaging/windows/AppxManifest.xml.in`) |

Once added, wire them in `App/CMakeLists.txt` (a `.rc` file on Windows)
and install the PNG to
`share/icons/hicolor/256x256/apps/` in `cmake/packaging/Linux.cmake`.
