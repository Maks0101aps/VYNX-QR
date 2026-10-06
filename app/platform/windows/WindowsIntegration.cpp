#include "platform/PlatformIntegration.h"

#include <QApplication>
#include <QFontDatabase>
#include <QStyleHints>
#include <QUrl>

#include "vynx-qr-bridge/src/lib.rs.h"

#include <dwmapi.h>
#include <windows.h>

namespace PlatformIntegration {

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

QString platformName() { return QStringLiteral("Windows"); }
QColor systemAccentColor() {
  const auto color = vynx::system_info().accent_color;
  return QColor::fromString(QString::fromUtf8(color.data(), static_cast<qsizetype>(color.size())));
}
bool systemPrefersDarkMode() {
  return QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}
QString systemFontFamily() {
  const auto families = QFontDatabase::families();
  for (const auto &name : {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI")}) {
    if (families.contains(name, Qt::CaseInsensitive)) return name;
  }
  return QStringLiteral("Segoe UI");
}
QString qtLicenseNotice() {
  return QStringLiteral("Uses Qt under LGPL-3.0. Licence texts, Qt source and\n"
                        "library replacement instructions are included in licenses/qt.");
}
} // namespace PlatformIntegration
