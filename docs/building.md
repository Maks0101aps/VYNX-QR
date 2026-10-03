# Build and packaging

## Prerequisites

| | |
| --- | --- |
| Rust | 1.82 or newer, `x86_64-pc-windows-msvc` |
| C++ | MSVC build tools with the Windows 10/11 SDK |
| CMake | 3.24 or newer |
| Qt | 6.5 or newer, MSVC build |
| NSIS | only for the installer target |

CMake drives Cargo. There is no separate Rust build step, and no `.lib` to copy by
hand.

## Qt

Point `QTDIR` at the installation, or pass `-DCMAKE_PREFIX_PATH=...` to CMake
directly:

```powershell
$env:QTDIR = "C:\Qt\6.8.3\msvc2022_64"
```

Only `Core`, `Gui` and `Widgets` are used. No WebEngine, no QML, no Network.

## Presets

```powershell
cmake --preset debug      # engine and the C++ bridge test
cmake --build --preset debug
ctest --preset debug

cmake --preset release            # the application
cmake --build --preset release

cmake --build --preset release-installer   # installer and portable archive
```

### Why there is no application in the debug preset

Rust on the MSVC target links the release C runtime in every profile, and there
is no supported way to ask for the debug one. A Qt debug build links the debug
runtime, so the two cannot coexist in one process. The application is therefore
built Release, and the debug preset covers the engine and the bridge test, which
pull in no Qt at all.

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