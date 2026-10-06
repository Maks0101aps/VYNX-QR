#include "platform/PlatformIntegration.h"

#include <QApplication>
#include <QFont>
#include <QStyleHints>

namespace PlatformIntegration {
QString platformName() { return QStringLiteral("Linux"); }
void applyWindowChrome(void *, bool) {}
QColor systemAccentColor() { return systemPalette().color(QPalette::Highlight); }
bool systemPrefersDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  const auto scheme = QApplication::styleHints()->colorScheme();
  if (scheme != Qt::ColorScheme::Unknown) return scheme == Qt::ColorScheme::Dark;
#endif
  return systemPalette().color(QPalette::Window).lightness() < 128;
}
QString systemFontFamily() { return QApplication::font().family(); }
QString qtLicenseNotice() {
  return QStringLiteral("Uses dynamically linked Qt under LGPL-3.0. System packages provide\n"
                        "Qt libraries and licences; AppImage includes Qt notices and source.\n"
                        "See share/doc/vynx-qr for dependency notices.");
}
} // namespace PlatformIntegration
