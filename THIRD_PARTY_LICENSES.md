# Third party components

VYNX QR itself is covered by the VYNX QR Source Available Licence in `LICENSE`.
That licence does not reach the components below, which keep their own.

## Qt 6

Qt is used under the **GNU Lesser General Public License version 3** and, at your
option, the **GNU General Public License version 3**.

- Qt 6 Widgets: `Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `platforms/qwindows.dll`
- Licence text: <https://www.gnu.org/licenses/lgpl-3.0.html>
- Qt sources: <https://download.qt.io/official_releases/qt/6.8/>

### How this project complies

Qt is **dynamically linked**. The application links `Qt6Core.dll`, `Qt6Gui.dll`
and `Qt6Widgets.dll`, which are shipped separately in the installer and in the
portable archive. Nothing from Qt is compiled into `VYNX QR.exe`.

This matters in two directions. Dynamic linking is what the LGPL prefers for an
application, and it also keeps the executable replaceable and updatable on its
own. Qt is deliberately **not** statically linked: it would require either
reimplementing its plugin loading or making the platform plugin a build-time
choice, and it would change the licensing obligations.

Qt's source is not modified. The application uses only documented public API.

The deployed runtime was audited and trimmed to what this application can
actually reach. Qt6Network, Qt6Svg, the image format plugins, the icon engine
plugins, the TLS backends and the DirectX shader compiler are not shipped,
because this application opens no socket, writes its own SVG as text, and
receives images as already decoded pixels.

## Rust crates

The engine links these crates. Each is permissive.

| Crate | Licence |
| --- | --- |
| `qrcode` | MIT |
| `rqrr` | MIT |
| `image` | MIT / Apache-2.0 |
| `serde` | MIT OR Apache-2.0 |
| `serde_json` | MIT OR Apache-2.0 |
| `thiserror` | MIT OR Apache-2.0 |
| `base64` | MIT OR Apache-2.0 |
| `cxx` | Apache-2.0 OR MIT |
| `cxx-build` | Apache-2.0 OR MIT |
| `windows` | MIT OR Apache-2.0 |

The exact versions are in `Cargo.lock`, which also records the resolved licence
metadata for each dependency.

## Build tooling

Used to build, not shipped:

| Tool | Licence |
| --- | --- |
| CMake | BSD-3-Clause |
| NSIS | zlib/libpng, historically used by the NSIS project |

## What is not here

No JavaScript runtime, no browser engine, no web view, no analytics, no crash
reporting service and no network client. The shipped application consists of one
executable and three Qt DLLs.