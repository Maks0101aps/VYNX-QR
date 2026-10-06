# Linux performance baseline

These are native Ubuntu 24.04 GitHub-runner measurements with system Qt 6.4.2,
GNU C++ 13.3.0 and an X11 window under Xvfb/Openbox. They describe this runner,
not consumer desktop performance. RSS/PSS must not be treated as Windows private
bytes. A warm build runner is not a cold consumer boot.

Evidence: [CI run 37457696714](https://github.com/Maks0101aps/VYNX-QR/actions/runs/37457696714),
commit `96a10e9b5069c5a60221e7595ef6e8e7e4041cde`, artifact
`ubuntu-native-evidence`, `linux-release/window-smoke.json`.

| Metric | Measured result |
| --- | --- |
| Application processes | 1, no child processes in every sample |
| RSS | 40,348–40,616 KiB |
| PSS | 29,589–29,765 KiB |
| Idle CPU | 0 ticks over each one-second observation |
| First observed startup | 112.28 ms, binaries already built on the runner |
| Warm startup median | 113.60 ms, four subsequent launches |
| Close | Normal window-manager close; exit status 0, all five runs |

Timing uses process start to a registered application window. First paint and
interactive GNOME/KDE startup have not been separately measured. Package smokes
retain their own fresh-runner runtime reports; these numbers are not substituted
for package acceptance.

## Windows regression observation

On the Windows 11 development machine, build 26200, the post-refactor binary
created one process without children in five runs. Private bytes were
35,172,352–36,298,752 bytes (33.54–34.62 MiB); startup was 382–433 ms. Idle CPU
was 0 or 0.015625 CPU seconds over each two-second sample. These are new
observations, not a statistically controlled comparison against 1.0.1.

The raw local report is `build/release-validation/port-windows-runtime.json`.
The helper also supports Windows portable artifact testing and writes a JSON
report instead of permanent application logs.
