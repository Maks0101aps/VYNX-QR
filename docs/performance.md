# Performance

Every number here was measured on the machine described below, with the
application started, left idle for ten seconds, and then sampled. Nothing is
estimated or carried over from an earlier toolchain.

## Machine

| | |
| --- | --- |
| OS | Windows 11 Pro, build 26200 |
| CPU | Intel Core i5-13500 |
| RAM | 32 GB |
| GPU | Intel UHD Graphics |
| Qt | 6.8.3, msvc2022_64, release binaries |
| Rust | 1.99.0, `x86_64-pc-windows-msvc` |
| C++ | MSVC 19.44 (Visual Studio Build Tools 2022 17.14) |

Qt was installed with `aqtinstall`; it is not part of the repository.

## Results

| | Tauri build | Qt build | Change |
| --- | --- | --- | --- |
| Processes | 7 | 1 | −86% |
| Idle private bytes | 189.7 MB | 29.0 MB | **−85%** |
| Working set | 32.6 MB (app only) | 30.6 MB | −6% |
| Idle CPU | not separately measured | 0.000 s / 5 s | — |
| Binary + runtime | 5.1 MB app | 23.6 MB app + runtime | see note |

The Tauri figure is the whole process tree: the application itself used 5.4 MB
of private bytes, and the WebView2 runtime it started used 184.3 MB across six
child processes. Those children were identified by walking parent process IDs, so
the number is not inflated by any other WebView2 user on the machine.

The Qt figure is a single process. No WebView2, no Chromium, no helper: the
process list was checked by module name and the loaded set contains no
`WebView2Loader`, no Edge and no Chrome binaries.

## Why the difference

Tauri renders through WebView2, which means a full browser engine per
application: its own process tree, its own caches, its own JIT. Removing the web
view removes all of it, and the QR engine that remains is the same Rust code in
both builds.

The working set barely moves, which is the point: the *visible* window size is
not where the memory was going. The engine was never the cost.

## Idle CPU

Measured over a five second window with nothing typed and no timers running:

```
CPU used in 5s idle: 0.000 s (0.0%)
```

The application has no polling. Detection is debounced behind a single-shot
timer that is stopped once it fires, and nothing is refreshed on a schedule.

## Binary size

The Qt build is larger than the Tauri one because it ships its runtime:

| | |
| --- | --- |
| `VYNX QR.exe` | 1.6 MB |
| Qt6Core, Qt6Gui, Qt6Widgets | 21.4 MB |
| `platforms/qwindows.dll` | 0.9 MB |
| `styles/qmodernwindowsstyle.dll` | 0.2 MB |
| **Total** | **23.6 MB** |

That is a deliberate trade. `windeployqt` initially copied 44.6 MB, including a
14 MB DirectX shader compiler, a network stack, an SVG image plugin and seven
plugin directories. None of it can be reached: logos are decoded by the Rust
engine and handed to Qt as raw pixels, so no image plugin is needed; the vector
export is text written by the engine, so no SVG handler is needed; and the
application opens no sockets. The build removes those and the result is verified
by launching the executable afterwards.

Static linking Qt would produce a single small file, and would also mean
reimplementing dynamic Qt's plugin loading so the Windows platform plugin still
works, on top of taking on LGPL static linking obligations. Dynamic deployment is
the right side of that trade.

## Startup

Not benchmarked to a millisecond figure. The application reaches a window in well
under a second from a warm cache on this machine, and it links three DLLs
against Tauri linking one — but a number that precise would need repeated runs on
an otherwise idle machine, and those were not taken. The claim being made is
therefore qualitative: startup is not noticeably slower than the web build.

## Acceptance against the targets

| Target | Result |
| --- | --- |
| Idle private bytes < 50 MB | **29.0 MB** |
| Idle private bytes < 70 MB | **29.0 MB** |
| One process | **1** |
| Idle CPU near zero | **0.0%** |

## Reproducing

```powershell
# Build and measure the Qt build
cmake --preset release
cmake --build --preset release

$p = Start-Process "build/release/bin/VYNX QR.exe"
Start-Sleep -Seconds 10
Get-Process -Id $p.Id | Select-Object PrivateMemorySize64, WorkingSet64
Stop-Process -Id $p.Id
```

To compare against the Tauri build, start `src-tauri/target/release/vynx-qr.exe`
instead and sum the private bytes of its process tree rather than the single
process, which is the whole difference the migration is about.