# Linux manual QA gate

Status: **pending**. No accessible GNOME/KDE Wayland desktop was available during
the port. Xvfb and any future headless Weston results are automated evidence,
not a claim of manual desktop validation.

Use the package downloaded from the candidate's Actions run, record its SHA256,
distro, desktop version, Qt version, GPU and session type (`echo "$XDG_SESSION_TYPE"`).
Launch as an ordinary user. Never substitute a build-tree executable.

| Check | Expected result | Manual result |
| --- | --- | --- |
| Launch and close | Native window; close ends the application | Pending |
| Smart URL, Email, Phone, Text | Correct detection, reversible original payload | Pending |
| Unicode and whitespace | Exact original content survives encoding | Pending |
| Wi-Fi, Contact, SMS, Geo | All forms produce scan-verified payloads | Pending |
| Colours, logo and shapes | Preview updates, valid logo accepted | Pending |
| PNG and SVG | Chosen dimensions, files at Unicode paths with spaces | Pending |
| Clipboard | One startup suggestion, copy pastes an image in another app | Pending |
| Settings and theme | Immediate changes; persistence at the XDG path | Pending |
| System theme | Follows the desktop's appearance and highlight colour | Pending |
| About and browser link | Correct platform/version; user-triggered browser only | Pending |
| Drag and drop | Logo from file manager, including Unicode/spaces | Pending |
| Native file dialog | GNOME/KDE portal or native dialog, fallback usable | Pending |
| Scaling | 100%, 125%, 150%, 200%; readable and reachable controls | Pending |
| Fonts | Noto Sans, Ubuntu, DejaVu Sans; no clipped required controls | Pending |
| Removal | Package paths removed, user preferences preserved | Pending |

At least one real GNOME Wayland or KDE Wayland session must complete the matrix
before declaring READY FOR v1.1.0. Record any unavailable additional desktop or
font configurations as untested rather than inferring success.
