#include <QtTest>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "setup/SetupWindow.h"
#include "setup/SetupBackend.h"

class SetupTest : public QObject {
  Q_OBJECT
private slots:
  void rejectsUnsafePaths() {
    QVERIFY(!SetupBackend::pathError(QString()).isEmpty());
    QVERIFY(!SetupBackend::pathError(QStringLiteral("relative/folder")).isEmpty());
    QVERIFY(!SetupBackend::pathError(QDir::rootPath()).isEmpty());
    QTemporaryDir dir;
    QVERIFY(SetupBackend::pathError(dir.filePath(QStringLiteral("QR тест"))).isEmpty());
  }
  void missingPayloadNeverSucceeds() {
    SetupBackend backend(QStringLiteral("/missing/vynx-payload"));
    QSignalSpy done(&backend, &SetupBackend::finished);
    QTemporaryDir dir;
    backend.install(dir.filePath(QStringLiteral("QR")));
    QTRY_COMPARE(done.count(), 1);
    QVERIFY(!done.first().at(0).toBool());
    QVERIFY(!done.first().at(1).toString().isEmpty());
  }
  void navigationAndErrorRecovery() {
    SetupWindow window(QStringLiteral("/missing/vynx-payload"));
    window.show();
    auto *next = window.findChild<QPushButton *>(QStringLiteral("next"));
    auto *back = window.findChild<QPushButton *>(QStringLiteral("back"));
    auto *folder = window.findChild<QLineEdit *>(QStringLiteral("folder"));
    QVERIFY(next && back && folder);
    QCOMPARE(window.step(), 0);
    QVERIFY(!back->isVisible());
    if (qEnvironmentVariableIsSet("VYNX_SETUP_SCREENSHOTS"))
      QVERIFY(window.grab().save(QStringLiteral("setup-welcome.png")));
    next->click();
    QCOMPARE(window.step(), 1);
    folder->clear();
    QVERIFY(!next->isEnabled());
    QTemporaryDir dir;
    folder->setText(dir.filePath(QStringLiteral("QR")));
    QVERIFY(next->isEnabled());
    if (qEnvironmentVariableIsSet("VYNX_SETUP_SCREENSHOTS"))
      QVERIFY(window.grab().save(QStringLiteral("setup-folder.png")));
    back->click();
    QCOMPARE(window.step(), 0);
    next->click();
    next->click();
    QTRY_VERIFY(next->isEnabled());
    QCOMPARE(window.step(), 2);
    QVERIFY(back->isVisible());
    back->click();
    QCOMPARE(window.step(), 1);
  }
#ifdef Q_OS_LINUX
  void linuxLifecycle();
#endif
};

#ifdef Q_OS_LINUX
void SetupTest::linuxLifecycle() {
  QTemporaryDir temp;
  qputenv("XDG_DATA_HOME", temp.filePath(QStringLiteral("data")).toUtf8());
  const auto source = temp.filePath(QStringLiteral("source.AppImage"));
  QFile file(source);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("payload fixture");
  file.close();
  const auto destination = temp.filePath(QStringLiteral("QR тест % $ folder"));
  SetupBackend backend(source);
  QSignalSpy done(&backend, &SetupBackend::finished);
  backend.install(destination);
  QTRY_COMPARE(done.count(), 1);
  QVERIFY2(done.first().at(0).toBool(), qPrintable(done.first().at(1).toString()));
  QFile installed(destination + QStringLiteral("/vynx-qr.AppImage"));
  QVERIFY(installed.open(QIODevice::ReadOnly));
  QCOMPARE(installed.readAll(), QByteArray("payload fixture"));
  installed.close();
  QFile foreign(destination + QStringLiteral("/my-file.txt"));
  QVERIFY(foreign.open(QIODevice::WriteOnly));
  foreign.write("keep");
  foreign.close();
  QString error;
  QVERIFY2(SetupBackend::uninstallLinux(destination, &error), qPrintable(error));
  QVERIFY(QFile::exists(foreign.fileName()));
  QVERIFY(!QFile::exists(installed.fileName()));
  QVERIFY(!QFile::exists(temp.filePath(QStringLiteral("data/applications/vynx-qr-local.desktop"))));
  done.clear();
  backend.install(destination);
  QTRY_COMPARE(done.count(), 1);
  QVERIFY(!done.first().at(0).toBool()); // Never adopt a foreign directory.
  QVERIFY(QFile::exists(foreign.fileName()));
}
#endif

QTEST_MAIN(SetupTest)
#include "Setup_test.moc"
