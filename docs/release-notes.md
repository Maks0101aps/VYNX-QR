# VYNX QR 1.1.4.1

Hotfix release for the Windows auto-updater.

### 🐛 Fixed
- **Windows Auto-Updater Launch Fix**: Resolved an issue where updating on Windows resulted in a `Windows cannot find "\\"` error popup. The updater now generates a clean standalone batch script in `%TEMP%` (`vynx_updater.cmd`) that uses `ping` to wait for process exit and launches the silent installer without `cmd.exe` command line quote-escaping issues.

---

# VYNX QR 1.1.4

This release adds multi-language localization across the entire application and installer, with English as the canonical default, alongside Russian and Ukrainian.

### 🌐 3-Language Localization (English, Russian, Ukrainian)
- **English Default**: The entire application and setup wizard default to English.
- **Language Switcher**: Switch dynamically between English, Russian, and Ukrainian in the Settings dialog (`Settings > Language`), with instant live retranslation of all UI elements without restarting.
- **Multilingual Setup Wizard & NSIS Installer**:
  - Branded Qt setup wizard includes a language picker in the sidebar.
  - Windows NSIS installer (`vynx_qr.nsi`) includes full multilingual string tables and language selection for English, Russian, and Ukrainian.
- **Consistent UI Copy**: All dialogs (About, Settings, Update, Scan Result, Screen Snipper, Special Payloads) are fully localized with no stray or untranslated labels.

### 🚀 Auto-Updater & Bug Fixes
- Fixed language inconsistency in About dialog update button.
- Version bump to 1.1.4 across all project components (CMake, Rust core, Rust CXX bridge, and Windows packaging).

---

# VYNX QR 1.1.3

This release introduces a built-in QR Code Scanner & Decoder, 4 new payload formats, an automatic update checking system (Auto-Updater), and general performance and reliability improvements.

### 🔍 QR Scanner & Image Decoder
- **Screen Area Snipping (`Ctrl+Shift+S`)**: Semi-transparent full-screen selection overlay to snip and decode any QR code on screen instantly.
- **Image File & Clipboard Decoding (`Ctrl+Shift+V`)**: Open any image file or paste an image from the clipboard to read QR codes directly.
- **Drag & Drop**: Drop any QR code image onto the window to decode it.
- **Scan Result Dialog**: View decoded text and content kind, copy with one click, open web links, or insert directly into the generator.
- Native Rust decoder powered by `rqrr` with luminance-based grayscale processing and alpha channel handling.

### 📋 4 New Structured QR Payload Formats
- **Calendar Event (iCalendar / VEVENT)**: Title, start/end date-time, location, and description.
- **2FA Authenticator (TOTP)**: Standard `otpauth://totp/` URIs for Google Authenticator, 1Password, Bitwarden, etc.
- **Cryptocurrency Payment**: Bitcoin (`bitcoin:`), Ethereum (`ethereum:`), TON (`ton://`), and USDT with address, amount, and message.
- **Bank Payment / SBP**: Standard Russian GOST R 56042-2014 / SBP QR code (`ST00012`).

### 🚀 Auto-Updater
- Background update check against GitHub releases at startup.
- User notification with one-click silent installation and seamless application restart on Windows.
- Manual check option available in the About dialog.

### 📦 Distribution & Platform Support
- Setup EXE, Portable ZIP, DEB, Arch packages, and AppImage remain available.
- Verify all downloads against `SHA256SUMS.txt`. Qt and Rust notices are included; bundled Qt includes corresponding source material.

**Real GNOME Wayland and KDE Plasma Wayland manual QA was not performed.**
Automated Linux GUI/package checks use X11/Xvfb. Clipboard, file-manager
drag-and-drop, native dialogs, system theme integration and fractional scaling
remain manually unverified on Wayland. Dedicated clean Windows 10 consumer-machine
validation is also pending. Windows/Linux x86_64 only; no ARM64 or macOS package.
