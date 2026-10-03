#include <QApplication>

#include "MainWindow.h"
#include "Theme.h"
#include "WindowsIntegration.h"

/// VYNX QR — a small Windows utility.
///
/// One process, no browser, no tray icon: closing the window ends the program.
int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QApplication::setApplicationName(QStringLiteral("VYNX QR"));
  QApplication::setOrganizationName(QStringLiteral("VYNX"));
  QApplication::setApplicationVersion(WindowsIntegration::versionString());
  QApplication::setApplicationDisplayName(QStringLiteral("VYNX QR"));

  QFont font(systemFontFamily());
  font.setPixelSize(13);
  QApplication::setFont(font);

  // The Windows accent, when the user has set one, drives the interactive
  // colours. Everything else stays VYNX blue.
  const auto info = vynx::system_info();
  QColor accent;
  if (!info.accent_color.empty()) {
    accent = QColor::fromString(QString::fromStdString(std::string(info.accent_color)));
  }

  const auto settings = vynx::load_settings();
  const Palette palette =
      makePalette(settings.theme == vynx::ThemeMode::Dark    ? QStringLiteral("dark")
                  : settings.theme == vynx::ThemeMode::Light ? QStringLiteral("light")
                                                             : QStringLiteral("system"),
                  settings.use_windows_accent ? accent : QColor());
  app.setPalette(QPalette(palette.window, palette.textPrimary));
  app.setStyleSheet(styleSheetFor(palette));

  MainWindow window;
  window.show();

  return app.exec();
}