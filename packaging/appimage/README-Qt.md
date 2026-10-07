# AppImage Qt and runtime replacement

This AppImage dynamically links the official, unmodified Qt 6.8.3 Linux runtime.
It includes Core, Gui, Widgets and their runtime dependencies/platform plugins.
The corresponding Qt Base, Qt SVG and Qt Wayland source archives are included
here with a SHA256 manifest. Full LGPL-3.0/GPL-3.0 notices accompany the source;
the source archives include their own upstream third-party notices.

Qt Wayland **client** integration is included. Qt Wayland Compositor, QML and
WebEngine libraries are prohibited by the package audit. No Qt GPL-only module
is intentionally linked into the application.

To inspect or modify the runtime, run `./VYNX-QR-*.AppImage --appimage-extract`.
Build a modified Qt 6.8 runtime following the corresponding source README,
preserving the Linux x86_64 ABI and required modules. Replace its shared libraries
under `squashfs-root/usr/lib` and plugins under `squashfs-root/usr/plugins`, then
run `squashfs-root/AppRun`. The application does not require a signature or key
and permits reverse engineering to debug modifications to LGPL libraries.

Non-Qt deployed library notices and provenance are in `licenses/system/`.
Corresponding source for LGPL system libraries is included there when bundled.
The GCC runtime-library exception, where applicable, is reproduced in full.

The Type 2 AppImage runtime is pinned to source commit
`8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa`; its source, libfuse 3.15.0 source,
squashfuse 0.5.2 source, patch/build recipe and static dependency notices are in
`licenses/appimage-runtime/`. The supplied runtime source's BUILD.md describes
rebuilding with a modified libfuse. A rebuilt runtime can be supplied to
appimagetool with `--runtime-file` when recreating the AppImage. No application
licence restriction overrides those dependency rights.

FUSE mount support is an AppImage packaging requirement; it is not a Qt package
dependency. Extraction mode is available where FUSE cannot be used. Reports
distinguish the QR application process from the AppImage mount/bootstrap runtime.
The application itself creates no worker processes and opens no network client.
