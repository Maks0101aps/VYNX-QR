# Windows installer acceptance

Status on 2026-10-25: **incomplete**. Source inspection and Qt tests do not
establish packaged installer behavior on Windows 10 or Windows 11.

The existing [local smoke report](evidence/setup-windows.json) records a package
named `VYNX-QR-Setup-x64-1.1.1.exe`, SHA256
`68d515cd9d06f6895c00ed5d8c0f7c75d521c5004a89a7e19f0358b14e73e18d`.
It covers silent installation into a path containing spaces and Cyrillic,
foreign-directory rejection, owned partial-install recovery, registration,
application launch/close, and uninstall preserving unrelated files.
It does not record an OS version or qualify the 1.1.2 release package.

Run the following matrix separately on Windows 10 and Windows 11 using the
actual candidate package, as an ordinary user. Use disposable test accounts or
VMs for failure scenarios. Preserve existing installations and user files.

| Scenario | Acceptance criterion | Windows 10 | Windows 11 |
| --- | --- | --- | --- |
| Fresh GUI installation | Wizard completes, registered application launches, no elevation | Pending | Pending |
| Silent `/S` and final `/D` | Successful exit, requested directory and correct registration | Pending | Pending |
| Unicode path with spaces | Installation, launch and uninstall succeed at the selected path | Pending | Pending |
| Upgrade from previous release | New version launches; preferences and unrelated files survive | Pending | Pending |
| Cancel before installation | Cancel button, Escape and window close leave no installed payload or registration | Pending | Pending |
| Close during installation | Observe the busy-state close guard; backend result remains visible and no success is reported early | Pending | Pending |
| Insufficient space | Clear failure, no false completion; existing installation and unrelated files remain intact | Pending | Pending |
| Damaged package | Extraction or installation fails without reporting success; record any partial files and recovery behavior | Pending | Pending |
| Uninstall while app is running | Record handling of locked files, exit status and remaining registration; no false successful removal | Pending | Pending |
| Normal uninstall | Shipped files and owned integration removed; preferences and unrelated files preserved | Pending | Pending |

## Theme limitation

`app/setup/SetupWindow.cpp` sets fixed dark stylesheet colours. Automatic system
light/dark appearance is **not implemented** in the wizard. Changing the system
theme should be recorded when testing appearance, but cannot currently qualify
automatic theme support. The main application's theme checks cover a different
window.

## Evidence required for each run

Record the candidate commit, package filename and SHA256, Windows edition and
build, previous package version/hash for upgrades, chosen directory, command
line, process exit codes, results per row, and relevant screenshots or logs.
Keep failures and partial-install details. Mark unavailable scenarios untested.
An automated smoke result only covers its recorded artifact and environment.
