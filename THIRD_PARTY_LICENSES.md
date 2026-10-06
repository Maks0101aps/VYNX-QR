# Third party components

VYNX QR itself is covered by the VYNX QR Source Available Licence in `LICENSE`.
That licence does not reach the components below, which keep their own.

## Qt 6

Qt is used under the **GNU Lesser General Public License version 3** and, at your
option, the **GNU General Public License version 3**.

- Qt 6 Widgets: `Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `platforms/qwindows.dll`
- Full licence texts: `licenses/qt/LGPL-3.0.txt` and `GPL-3.0.txt`.
- Complete corresponding Qt Base 6.8.3 source: bundled under `licenses/qt/`.
- Source provenance, hash, build and DLL replacement instructions: `licenses/qt/README.md`.

Qt is dynamically linked and unmodified. Users may replace the Qt DLLs and
plugins and reverse engineer the combined application to debug modifications
to LGPL libraries; the VYNX licence explicitly preserves these rights.
The About dialog identifies the use of Qt and the bundled licence information.

Qt's source archive also contains notices for its bundled third party code.
The package targets enforce the pinned Qt version and source hash.

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

The exact versions are in `Cargo.lock`, which records the resolved versions and checksums.

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