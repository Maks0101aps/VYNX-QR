# VYNX QR 1.1.3

This release introduces an automatic update checking system (Auto-Updater)
upon application start, along with general reliability improvements.

- Auto-Updater: checks for new releases on GitHub in a background thread at launch.
- If an update is available, prompts the user with release notes and enables a one-click update.
- On Windows: downloads the installer and runs silent setup (`/S`) before seamlessly restarting.
- Built-in manual update check available directly from the About dialog.
- Preserves all preferences, settings, and user configuration during updates.
- Setup EXE, Portable ZIP, DEB, Arch packages, and AppImage remain available.

Verify all downloads against `SHA256SUMS.txt`. Qt and Rust notices are included;
bundled Qt includes corresponding source material.

**Real GNOME Wayland and KDE Plasma Wayland manual QA was not performed.**
Automated Linux GUI/package checks use X11/Xvfb. Clipboard, file-manager
drag-and-drop, native dialogs, system theme integration and fractional scaling
remain manually unverified on Wayland. Dedicated clean Windows 10 consumer-machine
validation is also pending. Windows/Linux x86_64 only; no ARM64 or macOS package.
