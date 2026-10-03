# Performance

Every number here was measured on the machine described below with the release
build, by starting the application, leaving it idle, and then sampling. Nothing is
estimated and nothing is carried over from another toolchain.

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

The feature-complete native build against the previous web view build:

| | Web view build | Native build | Change |
| --- | --- | --- | --- |
| Processes | 7 | **1** | −86% |
| Private bytes | 189.7 MB | **33.6 MB** | **−82%** |
| Working set | 32.6 MB (app only) | 91.2 MB | — |
| Idle CPU | not separately measured | **0.000 s / 5 s** | — |
| Browser modules loaded | Chromium via WebView2 | **none** | — |
| Deployed size | 5.1 MB (app only) | 23.8 MB | see below |

The web view figure is the whole process tree: the application itself used
5.4 MB of private bytes, and the WebView2 runtime it started used 184.3 MB across
six child processes. Those children were attributed by walking parent process IDs,
so the number is not inflated by any other browser on the machine.

The native build is a single process. The loaded module list was checked by name
and contains no `WebView2Loader`, no Edge and no Chrome binary.

Working set is reported for completeness but is not the figure to judge this by:
it includes pages shared with other processes and does not fall when a window is
minimised. Private bytes is the honest figure, and it is the one the budget
applies to.

## Startup

Ten consecutive launches, timed from `Start-Process` to the moment a main window
handle exists:

| Run | Time to window |
| --- | --- |
| 1 (cold: binaries and the Rust library not yet in the page cache) | 1431 ms |
| 2 | 423 ms |
| 3 | 390 ms |
| 4 | 456 ms |
| 5 | 362 ms |
| 6 | 363 ms |
| 7 | 379 ms |
| 8 | 376 ms |
| 9 | 363 ms |
| 10 | 423 ms |

Warm median: **376 ms**. Cold first launch: **1.43 s**, dominated by reading the
executable and the three Qt DLLs off a disk that had not cached them yet.

This is a real measurement of process start to visible window, not to first
paint, so it is an upper bound on what a person perceives.

## Idle behaviour

With the window open and nothing typed:

```
CPU used in 5 s idle: 0.000 s (0.0%)
Threads: 11
Child processes: 0
```

There is no polling. Detection is debounced behind a single-shot timer that stops
itself once it fires, the clipboard is read once at start-up and then never again,
and nothing is refreshed on a schedule.

## Deployed size

| | |
| --- | --- |
| `VYNX QR.exe` | 1.6 MB |
| Qt6Core, Qt6Gui, Qt6Widgets | 21.4 MB |
| `platforms/qwindows.dll` | 0.9 MB |
| `styles/qmodernwindowsstyle.dll` | 0.2 MB |
| **Total** | **23.8 MB** |

Larger than the web build because it ships its runtime, which is the trade made
deliberately. `windeployqt` initially copied 44.6 MB, including a 14 MB DirectX
shader compiler, a network stack, an SVG image plugin and seven plugin
directories. None of it is reachable: logos are decoded by the Rust engine and
handed to Qt as raw pixels, so no image plugin is needed; the vector export is
text written by the engine, so no SVG handler is needed; and the application
opens no sockets. The build removes the unreachable remainder and verifies the
result by launching the executable afterwards.

Static linking Qt would produce a single smaller file, and would also mean
reimplementing dynamic Qt's plugin loading so the Windows platform plugin still
works, on top of taking on LGPL static linking obligations. Dynamic deployment is
the right side of that trade, and it is what keeps Qt replaceable.

## Acceptance against the targets

| Target | Result |
| --- | --- |
| Idle private bytes < 50 MB | **33.6 MB** |
| Hard threshold < 70 MB | **33.6 MB** |
| One process | **1** |
| Idle CPU approximately zero | **0.0%** |
| No browser runtime | **confirmed by module list** |

Adding the forms, the customize panel and the dialogs moved private memory from
29 MB to 33.6 MB. That is the cost of the features, and it is well inside the
budget.

## Reproducing

```powershell
cmake --preset release
cmake --build --preset release

$p = Start-Process "build/release/bin/VYNX QR.exe" -PassThru
Start-Sleep -Seconds 12
Get-Process -Id $p.Id |
  Select-Object PrivateMemorySize64, WorkingSet64, @{n='Threads';e={$_.Threads.Count}}
(Get-CimInstance Win32_Process | Where-Object { $_.ParentProcessId -eq $p.Id }).Count
Stop-Process -Id $p.Id
```

To compare against the web view build, start the previous release instead and
sum the private bytes of its whole process tree rather than of the single
process. That difference is the entire point of the migration.