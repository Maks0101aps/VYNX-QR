#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <cstring>

#include "MainWindow.h"
#include "Theme.h"
#include "platform/PlatformIntegration.h"

/// VYNX QR — a small native desktop utility.
///
/// One process, no browser, no tray icon: closing the window ends the program.
int main(int argc, char *argv[]) {
  // Informational options must also work without a graphical display.
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0 ||
        std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      QCoreApplication app(argc, argv);
      app.setApplicationName(QStringLiteral("VYNX QR"));
      app.setApplicationVersion(PlatformIntegration::versionString());
      QCommandLineParser parser;
      parser.setApplicationDescription(QStringLiteral("Fast, private QR codes."));
      parser.addHelpOption();
      parser.addVersionOption();
      parser.process(app);
      return 0;
    }
  }
  QApplication app(argc, argv);
  PlatformIntegration::captureSystemPalette();
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  parser.process(app);

  QApplication::setApplicationName(QStringLiteral("VYNX QR"));
  QApplication::setOrganizationName(QStringLiteral("VYNX"));
  QApplication::setApplicationVersion(PlatformIntegration::versionString());
  QApplication::setApplicationDisplayName(QStringLiteral("VYNX QR"));

  QFont font(systemFontFamily());
  font.setPixelSize(13);
  QApplication::setFont(font);

  QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/vynx-qr.png")));
  MainWindow window;
  window.show();

  return app.exec();
}