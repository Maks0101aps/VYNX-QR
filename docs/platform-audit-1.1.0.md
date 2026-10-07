# Platform audit for 1.1.0

Base: v1.0.1. One Qt Widgets frontend, CXX bridge and Rust engine remain shared.

| Area | Finding | Port strategy |
| --- | --- | --- |
| QR, detection, payloads, logo decoding, export | Portable Rust | Keep one engine and existing tests |
| Clipboard, dialogs, drop URLs, browser opening | Portable Qt | Keep Qt APIs, test X11 and Wayland |
| DWM and registry accent/build number | Windows only | Select a Windows backend in CMake / Rust cfg |
| Theme and font | Windows font hardcoded; Qt colorScheme requires 6.5 | Platform facade; native Linux font and palette, Qt 6.4 fallback |
| Settings | APPDATA assumed | Preserve Windows path; Linux absolute XDG_CONFIG_HOME or HOME/.config |
| CMake | MSVC check, .lib, Win32 libraries, resources, windeployqt | Select platform sources/libraries; .a on Linux; Windows resource/deploy include |
| Packaging | NSIS, EXE and ZIP only | Windows targets preserved; Linux packaging after native GUI validation |
| UI wording | Windows accent, Windows-only About, clipboard error | Common wording, platform-specific licence notice |
| Tests | APPDATA isolation and C:/logo.png | Isolate both platforms; temporary Unicode paths with spaces |
| Licensing | cargo-about Windows executable/graph, Qt official 6.8.3 payload | Host tool and target-specific Rust graph; system Qt for DEB/Arch; corresponding source for AppImage |
| Workflows | Windows only | Native Ubuntu build and Xvfb first, then package build and separate artifact consumers |

Ubuntu 24.04 is the initial baseline. Qt 6.4 is needed for its system Qt package;
native palette fallback avoids relying on Qt 6.5 colorScheme. No support claim
is made for a distro until its actual package and runtime checks pass.

Local WSL2 cannot start because virtualization is unavailable. Native Linux
validation uses GitHub runners. A headless compositor does not substitute for
manual GNOME/KDE Wayland QA. The latter remains a release gate.

DEB smoke run 37458465801 stopped because Openbox advertised its manager
identity before it exposed `_NET_CLIENT_LIST`. The installer, desktop entry,
icons, dynamic links and 48-crate/101-notice Linux inventory passed first.
The smoke now observes the missing-client-list startup state until its original
deadline while checking that the app stays alive. Other wmctrl errors remain
fatal. Three independent Xvfb/Openbox startups must all pass; the failed report
is retained, and the workflow is not retried to manufacture a green result.

Arch run 37459931044 reached native compilation but failed to resolve CXX
support symbols from the Rust static archive. Its compiler/link commands used
makepkg's default `-flto=auto` on the CXX-generated objects. PKGBUILD disables
that additional GCC LTO pass (`!lto`) for this mixed Rust/C++ static-library
build. Rust's configured release LTO and distro hardening flags remain enabled.
Real makepkg, bridge/window tests and fresh Arch install/run/remove passed in
run 37461160557. The original failed build log remains evidence.

Arch's initial artifact contained Rust notices, but the clean container's
`NoExtract = usr/share/doc/*` prevented their installation. Required notices now
use `/usr/share/licenses/vynx-qr`; run 37461160557 confirmed the setting and the
installed notice inventory/hashes.

DEB run 37586039614 built one Debian 12 package and tested that exact SHA256 on
both fresh Debian 12 and Ubuntu 24.04 environments. Both install/GUI/close/remove
checks passed, including 15 launches per distro and retained user preferences.

AppImage's initial Qt 6.8.3 XCB test failed before packaging. The diagnostic
follow-up established missing `libxcb-shape.so.0` on the build runner. Adding the
actual SDK dependencies made the Qt tests pass in run 37586370670. Its next
failure was deployment of the staged ELF after CMake stripped build RPATH;
linuxdeploy now resolves the pinned official SDK via an explicit packaging-only
LD_LIBRARY_PATH. Fresh artifact launch remains the acceptance gate.

The first container job with a spaced working-directory failed in the Actions
runner's docker invocation (`No such container: tree`), before CMake ran. Sources
remain in `source tree with spaces`; a quoted shell cd avoids that runner issue
and keeps the actual spaced-source build check.


AppImage's source/provenance audit found ICU 73 from the official Qt SDK rather
than Ubuntu's ICU package. The packaging step probes its exact 73.2 version and
retains upstream notices and the pinned source archive. The initial fresh launch
then found blacklisted GLVND entry points absent from the runner (`libOpenGL.so.0`,
then `libEGL.so.1`). The artifact now includes the generic GLVND dispatch set and
its original distro notices. No Qt package was added to the smoke runner to hide
those failures. Normal FUSE launch passed in CI run 37588399498 and the manual
Release run's AppImage smoke; all five closes also removed the mount daemon.


Manual Release run 37588424283 passed both Windows installer install/run/uninstall
checks, but both portable jobs rejected the Rust bundle's source-lock hash. The
repository lockfiles contained an unused local `vswhom-sys` patch record; unlocked
Cargo checks in the Windows packaging job removed that record. Removing exactly
that stale suffix from the source bridge lockfile reproduces the bundle hash
`fa191c68307b9a5f485e632cb5ebcf4d377c1866c18638b0ea52cf6f0b116aff`.
No production crate version or checksum changed. Both tracked lockfiles now omit
the unused build-machine patch marker, Cargo checks use --locked, and release
packaging rejects any remaining lockfile diff. Portable source-hash verification
is retained unchanged; failed installer-job reports and logs remain available.
