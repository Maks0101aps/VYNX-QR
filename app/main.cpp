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

  MainWindow window;
  window.show();

  return app.exec();
}