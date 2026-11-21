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
