# Qt runtime and corresponding source

VYNX QR dynamically links unmodified Qt 6.8.3 Core, Gui and Widgets and ships
the qwindows and qmodernwindowsstyle plugins from Qt Base. These components
are used under LGPL-3.0. Copyright The Qt Company Ltd. and other contributors.

Full LGPL-3.0 and GPL-3.0 texts are included beside this file. The complete
Qt Base 6.8.3 source archive, including its bundled third party notices and
build instructions, is shipped here as qtbase-everywhere-src-6.8.3.tar.xz.
It is copied without modifications from:
https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
SHA256: 56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80

To use a modified Qt, extract the source archive, follow its README build
instructions with MSVC x64 and a release build, and keep the Qt 6.8 ABI and
the Core, Gui, Widgets and Windows plugins enabled. Close VYNX QR, back up its
runtime files, then replace Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll,
platforms/qwindows.dll and styles/qmodernwindowsstyle.dll in its directory
with the matching modified libraries and plugins. Restart VYNX QR.
The per-user install is under %LOCALAPPDATA%/Programs/VYNX QR; the portable
package can be extracted to any writable directory. No signature check,
activation key or administrator permission is required to replace Qt.

The VYNX application licence does not restrict modification of these libraries
or reverse engineering for debugging such modifications. These rights and
redistribution of Qt remain governed by its own licences.
