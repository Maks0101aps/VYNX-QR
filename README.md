# VYNX QR

**Fast, private QR codes for Windows.**

VYNX QR is a small desktop QR code generator built with Tauri 2, React 19 and Rust.
Everything happens on your machine: no account, no telemetry, no network calls, no
history of what you encoded. Close the window and it is gone.

---

## What it does

Type or paste anything. VYNX QR works out the type on its own and renders the code
immediately — there is no _Generate_ button.

| Input               | Encoded as                                                    |
| ------------------- | ------------------------------------------------------------- |
| `github.com`        | `https://github.com`, with a notice that the scheme was added |
| `hello@example.com` | `mailto:hello@example.com`                                    |
| `+380 99 123 4567`  | `tel:+380991234567`                                           |
| Anything else       | Plain text                                                    |
| Wi-Fi               | `WIFI:` payload from the credentials form                     |
| Contact             | vCard 3.0                                                     |
| Message             | `sms:` URI                                                    |
| Place               | `geo:` URI (RFC 5870)                                         |

Everything is detected by deterministic rules, so the same text always produces the
same classification. There is no model, no heuristic that changes between runs, and
nothing is guessed from context.

### Verified, not assumed

Every code is decoded again by an independent decoder (`rqrr`) **after** it has been
rasterised. The status line reports what actually happened:

- **Scan verified** — the rendered image was read back and matched the payload exactly.
- **QR could not be verified** — the decoder could not read the symbol (for example,
  inverted colours).
- **Verification mismatch** — the decoder read something different.

`Scan verified` is a measured fact, not a decoration. When a logo is present, the code
is also re-rendered at half size and verified again, because a logo that only survives
at full resolution is not reliable in the wild.

### Copy, save, customise

- **Copy** puts a real bitmap (`CF_DIB`) on the Windows clipboard, so it pastes into
  Word, Slack, Paint and a browser as an image — not as a file path.
- **Save** writes a true vector **SVG** (one `<rect>` per module run, resolution
  independent) or a raster **PNG**.
- **Customise** controls module shape, both colours, the quiet zone and an optional
  logo. Adding a logo raises error correction to High automatically and clears a plate
  so the logo never sits on top of unrelated data modules.

### Privacy

- No network requests. The CSP in `tauri.conf.json` blocks everything except the local
  bundle and the IPC bridge.
- The clipboard is read **once** at start-up, and only if you leave that enabled.
- Only preferences are stored, in `%APPDATA%\VYNX\QR\settings.json`. QR content is
  never written anywhere.

---

## Requirements

- Windows 10 1809 or newer (Windows 11 fully supported)
- [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) — already
  present on Windows 11; the installer offers to fetch it on Windows 10
- To build from source: Node.js 20.19+, Rust 1.82+, and the MSVC build tools

## Install

Download one of the release installers and run it. Both install per-user and need no
administrator rights.

- `VYNX QR_1.0.0_x64-setup.exe` — NSIS
- `VYNX QR_1.0.0_x64_en-US.msi` — WiX

## Keyboard shortcuts

| Shortcut   | Action                                    |
| ---------- | ----------------------------------------- |
| `Ctrl + C` | Copy the QR code image                    |
| `Ctrl + S` | Save in the default format                |
| `Ctrl + E` | Toggle the customise panel                |
| `Ctrl + ,` | Open settings                             |
| `Esc`      | Clear the input (or close the open panel) |

These are in-app shortcuts only. VYNX QR never registers a key combination with the
operating system.

---

## Development

```bash
npm install          # install the frontend dependencies
npm run tauri dev    # run the app with hot reload
```

### Checks

```bash
npm run typecheck    # tsc --noEmit
npm run lint         # eslint, zero warnings allowed
npm run format       # prettier
npm test             # vitest
npm run build        # typecheck + production frontend build

cd src-tauri
cargo clippy --all-targets
cargo test
cargo build --release
```

### Installers

```bash
npm run tauri build                       # NSIS + MSI
npm run tauri build --bundles nsis        # NSIS only
npm run tauri build --bundles msi         # MSI only
```

Artifacts land in `src-tauri/target/release/bundle/`.

### Windows resources

The executable must carry three resources: the application manifest, the icon and the
version block. Without the manifest the loader never binds
`Microsoft.Windows.Common-Controls` v6, so the `TaskDialogIndirect` import used by
the dialog plugin cannot be resolved and the process dies at load time with
`STATUS_ENTRYPOINT_NOT_FOUND` before any of our code runs. This is why
`build.rs` does not simply trust the build to succeed.

`tauri-build` compiles these through `rc.exe` or `windres`. On a machine with no
Microsoft resource compiler, a stub `llvm-rc` can exit with status 0 and write a
32-byte, section-less COFF object, which leaves the executable with an empty
`.rsrc` while the build still looks clean. `build.rs` detects that empty object and
rebuilds the same three resources with `winresource`, which writes the COFF object
directly in Rust and needs no external tool. On a normal MSVC setup nothing changes.

### Icons

Application icons and the installer bitmaps are generated without any binary asset
dependency:

```bash
npm run icons
```

---

## How it is put together

```
src/                    React front end
  components/           UI, one folder per component with a CSS module
  hooks/                Debouncing, theming, toasts, shortcuts
  lib/                  IPC wrappers and pure helpers
  types/                Hand written mirrors of the Rust types
src-tauri/src/
  detect.rs             Content classification
  formats/              One module per payload format
  qr/                   Payload model, matrix, raster, SVG, verification
  commands/             The Tauri command surface
  platform/windows.rs   Registry reads for build number and accent colour
  settings.rs           Preferences persistence
```

The QR engine is plain Rust with no Tauri types in sight, so it is testable on its
own. Tauri only wires it to the window. The IPC contract is a tagged `QrPayload`
enum — no untyped `serde_json::Value` anywhere — mirrored by hand in
`src/types/qr.ts` so the compiler catches drift.

### Rendering

Modules are rasterised on a module-aligned integer pixel grid, so the preview and the
export are pixel-identical in shape and the preview is only ever downscaled, never
upscaled. The SVG export emits genuine geometry in module units and embeds nothing
rasterised unless a logo is present.

### Error correction and contrast

The renderer reports the WCAG contrast ratio between the chosen foreground and
background and warns below the thresholds where a code becomes hard or impossible to
scan. Warnings are advisory: you can always export what you built.

---

## Licence

**VYNX QR Source Available Licence — © 2026 Maks0101aps.**

Free to use, not to be changed.

- You may use VYNX QR for anything, personally or commercially, free of charge.
- You may read, audit and study the source.
- You may copy and redistribute it **unmodified**, including charging for it.

You may **not** modify, fork, patch or derive a version from it, and you may not
remove the copyright or attribution notices, without the author's written
permission. To ask for permission, open an issue.

This is deliberately _not_ an OSI-approved licence: it withholds the right to
modify, which the Open Source Definition treats as non-negotiable. It is
source-available rather than open source in the strict sense. If you need a
permissively licensed build, ask — that is a conversation worth having.

VYNX QR is built on permissive open source dependencies which keep their own
licences; this one covers VYNX QR itself only. See `LICENSE` for the full terms,
and `Cargo.lock` / `package-lock.json` for dependency licences.
