#include "SetupBackend.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {
const auto marker = QStringLiteral("/.vynx-qr-install");
const auto imageName = QStringLiteral("/vynx-qr.AppImage");
QString desktopPath() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
      + QStringLiteral("/applications/vynx-qr-local.desktop");
}
bool writeFile(const QString &path, const QByteArray &data) {
  QSaveFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
QString desktopQuote(QString path) {
  path.replace(QStringLiteral("\\"), QStringLiteral("\\\\\\\\"));
  path.replace(QStringLiteral("\""), QStringLiteral("\\\\\""));
  path.replace(QStringLiteral("`"), QStringLiteral("\\\\`"));
  path.replace(QStringLiteral("$"), QStringLiteral("\\\\$"));
  path.replace(QStringLiteral("%"), QStringLiteral("%%"));
  return QLatin1Char('"') + path + QLatin1Char('"');
}
QByteArray ownership(const QString &directory) {
  return QByteArray("VYNX QR user installation\n") + directory.toUtf8() + '\n';
}
bool owned(const QString &directory) {
  QFile file(directory + marker);
  return file.open(QIODevice::ReadOnly) && file.readAll() == ownership(directory);
}
}

SetupBackend::SetupBackend(QString payload, QObject *parent)
    : QObject(parent), payload_(std::move(payload)) {
  connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
    const bool success = status == QProcess::NormalExit && code == 0
        && QFileInfo::exists(executable());
    if (success) emit progress(100);
    emit finished(success, success ? QString() : QStringLiteral("Установка не завершена (код %1). Проверьте папку и закройте VYNX QR перед повторной попыткой.").arg(code));
  });
  connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart)
      emit finished(false, QStringLiteral("Не удалось запустить установщик: ") + process_.errorString());
  });
}
SetupBackend::~SetupBackend() {
  if (worker_) { worker_->wait(); delete worker_; }
}

QString SetupBackend::defaultDirectory() {
#ifdef Q_OS_WIN
  const QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\VYNX QR"), QSettings::NativeFormat);
  return settings.value(QStringLiteral("InstallDir"),
      qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Programs/VYNX QR")).toString();
#else
  return QDir::homePath() + QStringLiteral("/.local/opt/vynx-qr");
#endif
}
QString SetupBackend::pathError(const QString &path) {
  const QFileInfo info(path);
  if (path.trimmed().isEmpty() || !info.isAbsolute() || QDir(path).isRoot())
    return QStringLiteral("Укажите полный путь к отдельной папке приложения.");
  for (const auto c : path) {
    if (c.unicode() < 32 || c == QLatin1Char('"'))
      return QStringLiteral("В пути есть недопустимые символы.");
  }
  if (info.exists() && !info.isDir()) return QStringLiteral("Этот путь занят файлом.");
  if (info.isSymLink()) return QStringLiteral("Выберите папку, которая не является символической ссылкой.");
#ifdef Q_OS_WIN
  // NSIS treats /D as a raw final command-line argument.
  if (path.contains(QLatin1Char('*')) || path.contains(QLatin1Char('?')) || path.endsWith(QLatin1Char('.')))
    return QStringLiteral("В пути есть недопустимые символы.");
#endif
  return {};
}
QString SetupBackend::executable() const {
#ifdef Q_OS_WIN
  return directory_ + QStringLiteral("/VYNX QR.exe");
#else
  return directory_ + imageName;
#endif
}

void SetupBackend::install(const QString &directory) {
  if (process_.state() != QProcess::NotRunning || (worker_ && worker_->isRunning())) return;
  directory_ = QDir::cleanPath(QDir::fromNativeSeparators(directory));
  const auto validation = pathError(directory);
  if (!validation.isEmpty()) { emit finished(false, validation); return; }
  if (!QFileInfo(payload_).isFile()) {
    emit finished(false, QStringLiteral("Файлы установки не найдены. Запустите Setup из скачанного установочного пакета."));
    return;
  }
#ifdef Q_OS_WIN
  QFile partialMarker(directory_ + QStringLiteral("/.vynx-qr-install"));
  const bool partialInstall = partialMarker.open(QIODevice::ReadOnly)
      && partialMarker.readAll() == QByteArray("VYNX QR Windows setup");
  partialMarker.close();
  if (QFileInfo::exists(directory_) && !QDir(directory_).isEmpty() &&
      !partialInstall && !(QFileInfo::exists(executable()) && QFileInfo::exists(directory_ + QStringLiteral("/Uninstall.exe")))) {
    emit finished(false, QStringLiteral("Папка содержит другие файлы. Выберите пустую папку или папку существующей установки VYNX QR."));
    return;
  }
  emit progress(-1); // NSIS does not provide a reliable percentage to its parent.
  process_.setProgram(payload_);
  process_.setArguments({});
  process_.setNativeArguments(QStringLiteral("/S /D=") + QDir::toNativeSeparators(directory_));
  process_.start();
#else
  if (worker_) { worker_->wait(); delete worker_; }
  const auto desktop = desktopPath(); // Resolve on the GUI thread, before worker startup.
  worker_ = QThread::create([this, desktop] {
    const auto fail = [this](const QString &message) { emit finished(false, message); };
    const bool updating = owned(directory_);
    if (QFileInfo::exists(directory_) && !QDir(directory_).isEmpty() && !updating) {
      fail(QStringLiteral("Папка содержит другие файлы. Выберите новую пустую папку.")); return;
    }
    QFile previousDesktop(desktop);
    if (previousDesktop.exists()) {
      if (!previousDesktop.open(QIODevice::ReadOnly) ||
          !previousDesktop.readAll().startsWith(QByteArray("# VYNX QR: ") + directory_.toUtf8() + '\n')) {
        fail(QStringLiteral("Ярлык VYNX QR уже принадлежит другой установке. Сначала удалите её.")); return;
      }
    }
    if (!QDir().mkpath(directory_) || !QDir().mkpath(QFileInfo(desktop).absolutePath())) {
      fail(QStringLiteral("Не удалось создать папку. Проверьте права доступа.")); return;
    }
    // Identify a partially completed installation so failures can be retried.
    if (!writeFile(directory_ + marker, ownership(directory_))) {
      fail(QStringLiteral("Не удалось записать файлы в выбранную папку.")); return;
    }
    QFile source(payload_);
    QSaveFile target(executable());
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
      fail(QStringLiteral("Не удалось открыть файлы. Проверьте права доступа.")); return;
    }
    qint64 copied = 0;
    while (!source.atEnd()) {
      const auto block = source.read(1024 * 1024);
      if (block.isEmpty() || target.write(block) != block.size()) {
        fail(QStringLiteral("Не удалось скопировать приложение. Проверьте свободное место на диске.")); return;
      }
      copied += block.size();
      emit progress(static_cast<int>(copied * 90 / qMax<qint64>(source.size(), 1)));
    }
    target.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                         | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
    if (!target.commit()) {
      fail(QStringLiteral("Не удалось сохранить приложение. Проверьте свободное место на диске.")); return;
    }
    const auto icon = directory_ + QStringLiteral("/icon.png");
    const auto entry = QByteArray("# VYNX QR: ") + directory_.toUtf8() + '\n' +
        QStringLiteral("[Desktop Entry]\nType=Application\nName=VYNX QR\nComment=Fast, private QR codes\nExec=%1\nIcon=%2\nTerminal=false\nCategories=Utility;\nActions=Uninstall;\n\n[Desktop Action Uninstall]\nName=Uninstall VYNX QR\nExec=%1 --uninstall\n")
            .arg(desktopQuote(executable()), QString(icon).replace(QLatin1Char('\\'), QStringLiteral("\\\\"))).toUtf8();
    if (!QFile::copy(QStringLiteral(":/icons/vynx-qr.png"), icon) && !QFileInfo::exists(icon)) {
      fail(QStringLiteral("Не удалось сохранить значок приложения.")); return;
    }
    if (!writeFile(directory_ + marker, ownership(directory_)) || !writeFile(desktop, entry)) {
      // Files remain identifiable for a retry; no unrelated directory is removed.
      fail(QStringLiteral("Приложение скопировано, но ярлык создать не удалось. Проверьте права и повторите установку.")); return;
    }
    emit progress(100);
    emit finished(true, {});
  });
  worker_->start();
#endif
}

bool SetupBackend::uninstallLinux(const QString &directory, QString *error) {
  if (!owned(directory)) { *error = QStringLiteral("Эта папка не принадлежит установщику VYNX QR."); return false; }
  QFile desktop(desktopPath());
  if (desktop.exists()) {
    if (!desktop.open(QIODevice::ReadOnly) ||
        !desktop.readAll().startsWith(QByteArray("# VYNX QR: ") + directory.toUtf8() + '\n')) {
      *error = QStringLiteral("Ярлык принадлежит другой установке."); return false;
    }
    desktop.close();
    if (!desktop.remove()) { *error = desktop.errorString(); return false; }
  }
  for (const auto &name : {imageName, QStringLiteral("/icon.png"), marker}) {
    QFile file(directory + name);
    if (file.exists() && !file.remove()) { *error = file.errorString(); return false; }
  }
  QDir().rmdir(directory); // Only removes an empty directory. Preferences are preserved.
  return true;
}
