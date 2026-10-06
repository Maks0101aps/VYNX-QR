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
