#include "WindowsIntegration.h"

#include <QDesktopServices>
#include <QUrl>

#include "vynx-qr-bridge/src/lib.rs.h"

#include <dwmapi.h>
#include <windows.h>

namespace WindowsIntegration {

void applyWindowChrome(void *window, bool dark) {
  if (window == nullptr) {
    return;
  }
  const HWND handle = static_cast<HWND>(window);
  if (handle == nullptr) {
    return;
  }

  // Rounded corners on Windows 11. Unsupported builds ignore the value, and the
  // window keeps its square corners, so there is nothing to fall back to.
  const BOOL useDark = dark ? TRUE : FALSE;
  const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
  const COLORREF defaultColour = DWMWA_COLOR_DEFAULT;
  DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
  DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
  DwmSetWindowAttribute(handle, DWMWA_BORDER_COLOR, &defaultColour, sizeof(defaultColour));
  DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR, &defaultColour, sizeof(defaultColour));
}

bool isWindows11() {
  // The engine already reads the build number from the registry, which is both
  // the same answer and one less Windows API to keep in step.
  return vynx::system_info().is_windows_11;
}

void openUrl(const QString &url) {
  // DesktopServices hands the URL to the shell, which routes it to the default
  // browser. No socket is opened by the app itself.
  QDesktopServices::openUrl(QUrl(url));
}

QString versionString() {
#ifdef VYNX_VERSION
  return QStringLiteral(VYNX_VERSION);
#else
  return QStringLiteral("1.0.1");
#endif
}

QString repositoryUrl() {
#ifdef VYNX_REPOSITORY
  return QStringLiteral(VYNX_REPOSITORY);
#else
  return QStringLiteral("https://github.com/Maks0101aps/VYNX-QR");
#endif
}

} // namespace WindowsIntegration
