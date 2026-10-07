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


## Downloaded package observations

[Run 37586039614](https://github.com/Maks0101aps/VYNX-QR/actions/runs/37586039614)
at commit `b15874a8f136ff525f71d586e6128f7b40d62ff3` installed the same DEB
SHA256 `4ba36488368eaf6ebdf01e1427e42a9a0a1dedb67b90bcb265acd2523c14b1ed`
on fresh Debian 12 and Ubuntu 24.04. These package jobs passed even though that
run's independent AppImage build failed; they are not an all-platform green claim.

| Artifact environment | Launches | RSS (KiB) | PSS (KiB) | Startup range / median |
| --- | --- | --- | --- | --- |
| Debian DEB on Ubuntu 24.04 | 15 | 40,400?40,656 | 29,644?29,840 | 56.84?110.05 / 57.82 ms |
| Same DEB on Debian 12 | 15 | 39,332?39,812 | 31,611?32,102 | 56.35?377.94 / 56.89 ms |
| Arch package on fresh Arch | 5 | 42,272?42,640 | 35,232?35,560 | 58.33?58.81 / 58.43 ms |

Each DEB job used three independent Xvfb/Openbox sessions and five launches per
session. Every application had zero children and exited normally on window close.
DEB idle CPU median was zero; an individual one-second sample occasionally
accounted for one CPU tick (about 1%), so not every sample is reported as exactly
zero. Arch's five idle samples were all zero. These short observations are not a
long-running CPU benchmark or a controlled comparison between distros.

The first Debian launch was 377.94 ms; later launches had warm caches. This is
not a consumer cold boot measurement. All observed application RSS values are
below the preferred 100 MB Linux budget without comparing RSS to Windows private
bytes. AppImage needs its own measurements and a separate count for any FUSE
mount helper; native package numbers cannot stand in for it.
