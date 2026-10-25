#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include "setup/SetupWindow.h"
#include <cstring>

#include "MainWindow.h"
#include "Theme.h"
#include "platform/PlatformIntegration.h"

static void setupOptions(QCommandLineParser &parser) {
  parser.addOption({QStringLiteral("setup"), QStringLiteral("Install VYNX QR for the current user.")});
  parser.addOption({QStringLiteral("backend"), QStringLiteral("Windows setup payload."), QStringLiteral("file")});
  parser.addOption({QStringLiteral("uninstall"), QStringLiteral("Remove a Linux per-user installation.")});
}

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
      setupOptions(parser);
      parser.process(app);
      return 0;
    }
  }
  QApplication app(argc, argv);
  PlatformIntegration::captureSystemPalette();
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  setupOptions(parser);
  parser.process(app);

  QApplication::setApplicationName(QStringLiteral("VYNX QR"));
  QApplication::setOrganizationName(QStringLiteral("VYNX"));
  QApplication::setApplicationVersion(PlatformIntegration::versionString());
  QApplication::setApplicationDisplayName(QStringLiteral("VYNX QR"));
  QGuiApplication::setDesktopFileName(QStringLiteral("vynx-qr"));

  QFont font(systemFontFamily());
  font.setPixelSize(13);
  QApplication::setFont(font);

  QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/vynx-qr.png")));
  const auto appImage = qEnvironmentVariable("APPIMAGE");
  if (parser.isSet(QStringLiteral("setup")) || QFileInfo(appImage).fileName().startsWith(QStringLiteral("VYNX-QR-Setup-"))) {
    auto payload = parser.value(QStringLiteral("backend"));
#ifdef Q_OS_LINUX
    payload = appImage;
#endif
    SetupWindow setup(payload);
    setup.show();
    PlatformIntegration::applyWindowChrome(reinterpret_cast<void *>(setup.winId()), true);
    return app.exec();
  }
  if (parser.isSet(QStringLiteral("uninstall"))) {
#ifdef Q_OS_LINUX
    const auto directory = QFileInfo(appImage).absolutePath();
    if (appImage.isEmpty()) return 1;
    if (QMessageBox::question(nullptr, QStringLiteral("Удаление VYNX QR"),
          QStringLiteral("Удалить VYNX QR и ярлык из меню приложений? Ваши настройки сохранятся.")) != QMessageBox::Yes) return 0;
    QString error;
    if (!SetupBackend::uninstallLinux(directory, &error)) {
      QMessageBox::warning(nullptr, QStringLiteral("Удаление VYNX QR"), error);
      return 1;
    }
    QMessageBox::information(nullptr, QStringLiteral("VYNX QR"), QStringLiteral("Приложение удалено."));
    return 0;
#else
    return 1;
#endif
  }
  MainWindow window;
  window.show();

  return app.exec();
}
