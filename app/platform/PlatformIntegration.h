#pragma once

#include <QColor>
#include <QPalette>
#include <QString>
#include <functional>

class QObject;

namespace PlatformIntegration {
QString versionString();
QString repositoryUrl();
void openUrl(const QString &url);
QString platformName();
QString qtLicenseNotice();
QString systemFontFamily();
QColor systemAccentColor();
bool systemPrefersDarkMode();
void applyWindowChrome(void *window, bool dark);
// Read before applying the application's own palette, avoiding theme feedback.
void captureSystemPalette();
const QPalette &systemPalette();
void setApplicationPalette(const QPalette &palette);
void observeSystemTheme(QObject *context, std::function<void()> changed);
} // namespace PlatformIntegration
