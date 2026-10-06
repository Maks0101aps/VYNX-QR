#pragma once

#include <QString>

#include "vynx-qr-bridge/src/lib.rs.h"

/// Small Windows specific behaviours that improve the app without being required
/// for it to work. Every call here is best effort: a Windows 10 host that refuses
/// one of these keeps the feature off and carries on.

namespace WindowsIntegration {

/// Apply the window's rounded corners and dark title bar.
///
/// Windows 11 draws these itself. On Windows 10 the call is ignored, which is why
/// nothing here is allowed to fail loudly.
void applyWindowChrome(void *window, bool dark);

/// True when the running host is Windows 11 or newer.
bool isWindows11();

/// Open a URL with whatever the user's default browser is.
///
/// Uses the shell rather than a network call, so it works without the app
/// linking Qt Network.
void openUrl(const QString &url);

/// The product version, as shown in the About dialog and the installer.
QString versionString();

/// The repository, shown in the About dialog.
QString repositoryUrl();

} // namespace WindowsIntegration
