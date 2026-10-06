#include "PlatformIntegration.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

namespace PlatformIntegration {
namespace {
QPalette nativePalette;
}
void captureSystemPalette() { nativePalette = QApplication::palette(); }
const QPalette &systemPalette() { return nativePalette; }
QString versionString() { return QStringLiteral(VYNX_VERSION); }
QString repositoryUrl() { return QStringLiteral(VYNX_REPOSITORY); }
void openUrl(const QString &url) { QDesktopServices::openUrl(QUrl(url)); }
} // namespace PlatformIntegration
