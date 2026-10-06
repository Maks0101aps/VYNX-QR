#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

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
} // namespace PlatformIntegration
