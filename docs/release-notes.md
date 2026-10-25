# VYNX QR 1.1.2

This release adds a dark, four-step graphical setup for Windows and Linux,
with folder selection, installation status, retry after an error and optional
first launch.

- Windows: per-user Setup EXE with the new Qt wizard. Silent `/S` and final
  `/D=<path>` remain supported. Existing install locations are remembered.
- Linux: a new `VYNX-QR-Setup-1.1.2-x86_64.AppImage` installs the application
  in your home directory and adds a menu entry with an Uninstall action.
  Keep the Setup filename or use `--setup` to open the wizard explicitly.
- Removal preserves unrelated files and preferences. Interrupted Windows
  installations can be retried, and uninstalling an older destination preserves
  integration owned by a newer installation.
- Portable Windows ZIP, ordinary AppImage, DEB and Arch packages remain available.

Verify all downloads against `SHA256SUMS.txt`. Qt and Rust notices are included;
bundled Qt includes corresponding source material.

**Real GNOME Wayland and KDE Plasma Wayland manual QA was not performed.**
Automated Linux GUI/package checks use X11/Xvfb. Clipboard, file-manager
drag-and-drop, native dialogs, system theme integration and fractional scaling
remain manually unverified on Wayland. Dedicated clean Windows 10 consumer-machine
validation is also pending. Windows/Linux x86_64 only; no ARM64 or macOS package.
