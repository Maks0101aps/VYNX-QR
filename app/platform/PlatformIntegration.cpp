#include "PlatformIntegration.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QStyleHints>
#include <QTimer>

namespace PlatformIntegration {
namespace {
QPalette nativePalette;
bool applyingPalette = false;
}
void captureSystemPalette() { nativePalette = QApplication::palette(); }
const QPalette &systemPalette() { return nativePalette; }
void setApplicationPalette(const QPalette &palette) {
  applyingPalette = true;
  QApplication::setPalette(palette);
  applyingPalette = false;
}
void observeSystemTheme(QObject *context, std::function<void()> changed) {
  QObject::connect(qApp, &QGuiApplication::paletteChanged, context,
      [changed](const QPalette &palette) {
        if (applyingPalette) return;
        nativePalette = palette;
        changed();
      });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  QObject::connect(QApplication::styleHints(), &QStyleHints::colorSchemeChanged,
      context, [context, changed] { QTimer::singleShot(0, context, changed); });
#endif
}
QString versionString() { return QStringLiteral(VYNX_VERSION); }
QString repositoryUrl() { return QStringLiteral(VYNX_REPOSITORY); }
void openUrl(const QString &url) { QDesktopServices::openUrl(QUrl(url)); }
} // namespace PlatformIntegration
