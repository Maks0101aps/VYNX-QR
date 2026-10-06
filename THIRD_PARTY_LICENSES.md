# Third party components

VYNX QR itself is covered by the VYNX QR Source Available Licence in `LICENSE`.
That licence does not reach the components below, which keep their own.

## Qt 6

Qt is used under the **GNU Lesser General Public License version 3** and, at your
option, the **GNU General Public License version 3**.

- Qt 6 Widgets: Core, Gui, Widgets and the platform plugins for the package's OS.
- Full licence texts: `licenses/qt/LGPL-3.0.txt` and `GPL-3.0.txt`.
- Windows: complete corresponding Qt Base 6.8.3 source bundled under `licenses/qt/`;
  provenance, hash, build and DLL replacement instructions in `licenses/qt/README.md`.
- DEB and Arch: Qt is dynamically linked from system packages. The distribution
  supplies its notices, corresponding source and updates. See the installed
  `share/doc/vynx-qr/README-licenses.md`.
- AppImage must include corresponding source for every bundled Qt module and
  library replacement instructions in its AppDir; these are a packaging gate.

Qt is dynamically linked. Users may replace the Qt libraries and
plugins and reverse engineer the combined application to debug modifications
to LGPL libraries; the VYNX licence explicitly preserves these rights.
The About dialog identifies the use of Qt and the bundled licence information.

Qt's source archive also contains notices for its bundled third party code.
Bundled-Qt package targets enforce their pinned Qt version and source hash.

## Rust crates

Windows packages include the generated bundle under
`licenses/rust/THIRD_PARTY_RUST_LICENSES.html`; Linux system packages use
`share/doc/vynx-qr/licenses/rust/`. It contains exact crate versions,
licence expressions, authors, complete licence texts and copyright notices.
Original upstream LICENSE, NOTICE, COPYING and COPYRIGHT files are also
preserved unmodified under `licenses/rust/notices/`.

`licenses/rust/manifest.json` records the bridge lockfile SHA256, selected
licences and SHA256 of every original notice file. Packaging regenerates the
bundle with pinned, hash-verified cargo-about 0.9.2 from `bridge/Cargo.lock`,
using the default-feature production dependency graph for the package target:
`x86_64-pc-windows-msvc` or `x86_64-unknown-linux-gnu`. The manifest records which.
Build-only and development-only dependencies are excluded. Production procedural
macro dependencies are retained conservatively; the inventory does not claim
that every crate contributes machine code after linker optimisation.

Licence resolution or a missing upstream notice file causes packaging to fail.

## Build tooling

Used to build, not shipped:

| Tool | Licence |
| --- | --- |
| CMake | BSD-3-Clause |
| NSIS | zlib/libpng, historically used by the NSIS project |
| cargo-about | MIT OR Apache-2.0 |

## What is not here

No JavaScript runtime, no browser engine, no web view, no analytics, no crash
reporting service and no network client. The executable runtime consists of one
application process and dynamically linked Qt runtime libraries; no browser
runtime or network client is shipped.
