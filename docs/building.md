# Build and packaging

## Windows prerequisites

| | |
| --- | --- |
| Rust | 1.82 or newer, `x86_64-pc-windows-msvc` |
| C++ | MSVC build tools with the Windows 10/11 SDK |
| CMake | 3.24 or newer |
| Qt | 6.5 or newer, MSVC build |
| NSIS | only for the installer target |
| Python | 3.9 or newer, for generating the packaged Rust licence bundle |

CMake drives Cargo. There is no separate Rust build step, and no `.lib` to copy by
hand.

Run the commands below from **Developer PowerShell for VS 2022** with the x64
toolchain selected. The presets use Ninja, which needs MSVC's `PATH`, `INCLUDE`
and `LIB` initialized before CMake runs. Setting `CMAKE_CXX_COMPILER=cl` alone
does not initialize that environment. CI and release workflows use
`ilammy/msvc-dev-cmd` with `arch: x64` for the same setup.

## Qt

Point `QTDIR` at the installation, or pass `-DCMAKE_PREFIX_PATH=...` to CMake
directly:

```powershell
$env:QTDIR = "C:\Qt\6.8.3\msvc2022_64"
```

Only `Core`, `Gui` and `Widgets` are used. No WebEngine, no QML, no Network.

## Presets

```powershell
cmake --preset release                  # the application
cmake --build --preset release
ctest --preset release                  # engine, bridge and window tests

cmake --preset debug                    # engine and C++ bridge test only
cmake --build --preset debug

cmake --build --preset release --target installer portable
```

Package targets regenerate `licenses/rust/` from the locked Windows production
dependency graph using cargo-about 0.9.2. Its official Windows binary is downloaded
into the build directory and checked against a pinned SHA256; it is not shipped.
The bundle includes readable HTML, an inventory with exact versions and hashes,
and original upstream licence/copyright/NOTICE files. Packaging fails on unresolved
licences or missing notice files. Full Qt Base 6.8.3 source is also downloaded and
hash-verified for inclusion, so packaging requires network access on the first run
and currently requires Qt 6.8.3. No licences are maintained as a manual crate table.

### Why the tests run in release

Two separate reasons, and both bite the same way.

Rust on the MSVC target links the release C runtime in every profile, and there
is no supported way to ask for the debug one. A Qt debug build links the debug
runtime, so the two cannot coexist in one process.

Worse, the Qt debug import libraries are not all debug: CMake still resolves
`Qt6Guid` to its release library, so a debug build ends up loading `Qt6Cored.dll`
and `Qt6Widgetsd.dll` beside `Qt6Guid.dll`. Two Qt copies in one process corrupt the
heap, and the only symptom is a bare `0xC0000374` with no diagnostic at all. That is
what it took to find, and it is why the window tests are built with the release
preset.

## Artifacts

```
build/release/bin/VYNX QR.exe      the application, with its Qt runtime beside it
build/release/package/VYNX-QR-Setup-x64-<version>.exe
build/release/package/VYNX-QR-Portable-x64-<version>.zip
```

The installer is per-user, creates a Start Menu entry and an Apps and Features
entry, and does not create a desktop shortcut. Uninstalling removes the
application and leaves preferences alone.

## The installer is hand written

`installer/vynx_qr.nsi`, not CPack's generator, for two reasons that both show up
the moment it is used here: the generated Start Menu shortcut points at the CMake
target name (`VYNX_QR.exe`) rather than at the real executable (`VYNX QR.exe`),
and the generator still requests administrator rights, which is wrong for a
per-user utility.

## The deployment is audited

`windeployqt` copies for the whole of Qt rather than for this program. The build
removes what cannot be reached and then verifies the result by starting the
executable:

| Removed | Why |
| --- | --- |
| `dxcompiler.dll`, `dxil.dll` | shader compiler, for rendering paths this app never takes |
| `Qt6Network.dll`, `tls/`, `networkinformation/` | no sockets are opened |
| `Qt6Svg.dll` | the vector export is text written by the engine |
| `imageformats/` | images reach Qt as decoded RGBA, never as files |
| `iconengines/`, `generic/` | not referenced |

`Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `platforms/qwindows.dll` and
`styles/qmodernwindowsstyle.dll` are kept: removing any of them stops the window
from being created.

## Icons and resources

`app/resources/vynx_qr.rc.in` is an ordinary Windows resource script. CMake runs
`cmake/ConfigureResource.cmake` to substitute the absolute icon path and the
version numbers, then the resource compiler builds it. It carries the icon, the
version block, and the manifest that binds Common Controls v6 — without that
manifest `TaskDialogIndirect` cannot be resolved and the process dies at load
time with `STATUS_ENTRYPOINT_NOT_FOUND`.

The icon set itself lives in `app/resources/icons/` and is checked in, so a
normal build needs nothing but the toolchain above. It can be regenerated with
`node scripts/generate-icons.mjs`, which is the one place Node is still useful
and is entirely optional.

## Linux native builds

Build natively on Linux x86_64 with CMake >=3.24, Ninja, GCC, Python >=3.9,
Rust stable and Qt >=6.4 Core/Gui/Widgets/Test. The locked graph needs a newer
compiler than the crate's historic rust-version metadata; Debian CI uses Rust
1.90, rather than Debian 12's older packaged compiler. Windows Qt packaging and
AppImage use the official Qt 6.8.3 SDK. No cross-compilation is performed.

On Debian 12 or Ubuntu 24.04:

```bash
sudo apt update
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-base-dev-tools qt6-wayland python3 xvfb xauth dpkg-dev
cmake --preset linux-release
cmake --build --preset linux-release
xvfb-run -a ctest --preset linux-release
cmake --build --preset linux-release --target deb
```

Use `linux-debug` for a full Linux debug build, including window tests. For an
interactive desktop run `build/linux-release/bin/vynx-qr`. Qt selects the session
backend; `QT_QPA_PLATFORM=xcb` and `QT_QPA_PLATFORM=wayland` can explicitly select
one for diagnostics. Headless Xvfb tests do not establish GNOME/KDE compatibility.

The DEB target uses CPack and system Qt. Runtime dependencies are inferred by
dpkg-shlibdeps; QPA and Wayland plugins are explicit dependencies. The public DEB
is built on Debian 12 and tested as the same downloaded artifact on Debian 12
and Ubuntu 24.04. Ubuntu's separate native build remains a compatibility check.

For Arch, install base-devel, cmake, ninja, rust, python, qt6-base, qt6-wayland,
xorg-server-xvfb and xorg-xauth. Commit the source first, then run as a normal user:

```bash
python scripts/prepare-arch-package.py --output build/arch-package --build
```

This renders the PKGBUILD with the product version and SHA256 of `git archive`
for the exact source commit. It never runs makepkg as root. Mandatory dependency
notices are installed under `/usr/share/licenses/vynx-qr`, so Arch's container
NoExtract setting for documentation cannot discard them.

For AppImage, install the official Linux Qt 6.8.3 `linux_gcc_64` SDK using pinned
aqtinstall 3.3.0, and set `QTDIR` to its `gcc_64` directory. The CI job lists the
native XCB/OpenGL build dependencies. Enable matching APT deb-src repositories:
bundled LGPL system libraries retain their corresponding source packages.

```bash
cmake --preset linux-appimage
cmake --build --preset linux-appimage
xvfb-run -a ctest --preset linux-appimage
cmake --build --preset linux-appimage --target appimage
```

The packaging script verifies pinned linuxdeploy, Qt plugin and runtime hashes,
keeps XCB/Wayland plugins, rejects WebEngine/QML/Quick/compositor libraries and
includes Qt Base/Wayland/SVG source archives plus runtime/system dependency
notices. The fresh-runner smoke launches the downloaded AppImage with FUSE; it
does not silently substitute an extracted executable after a runtime failure.

Linux output paths:

```text
build/linux-release/bin/vynx-qr
build/linux-release/package/vynx-qr_<version>_amd64.deb
build/arch-package/vynx-qr-<version>-1-x86_64.pkg.tar.zst
build/appimage/package/VYNX-QR-<version>-x86_64.AppImage
```

Product version comes from root CMake PROJECT_VERSION. Private, unpublished Rust
crate versions are independent. Every binary package includes its target-specific
locked production dependency notices, checked again after artifact download.

## Release verification

The manual Release workflow builds all platforms without publishing a GitHub
release. It invokes CI's native Linux build/package/smoke jobs, builds Windows
installer/portable artifacts, and tests both on windows-2022 and windows-2025.
Only after every required job succeeds does collection produce five binary
packages and one unified SHA256SUMS.txt. Tag-triggered publication creates a draft
and checks that the tag matches CMake's version. A failed package gate prevents
partial publication.

Do not tag v1.1.0 until the candidate is merged, the merge commit's main CI and
manual Release run are green, and the real desktop checklist in
[linux-manual-qa.md](linux-manual-qa.md) is completed. Current runner automation
cannot substitute for that manual gate.
