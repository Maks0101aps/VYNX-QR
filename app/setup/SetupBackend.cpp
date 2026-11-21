#include "SetupBackend.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include "i18n/I18n.h"

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
    emit finished(success, success ? QString() : I18n::tr("Installation not completed (code %1). Check the folder and close VYNX QR before retrying.").arg(code));
  });
  connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart)
      emit finished(false, I18n::tr("Failed to launch installer: %1").arg(process_.errorString()));
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
    return I18n::tr("Specify full path to a separate application folder.");
  for (const auto c : path) {
    if (c.unicode() < 32 || c == QLatin1Char('"'))
      return I18n::tr("Invalid characters in path.");
  }
  if (info.exists() && !info.isDir()) return I18n::tr("This path is occupied by a file.");
  if (info.isSymLink()) return I18n::tr("Choose a folder that is not a symbolic link.");
#ifdef Q_OS_WIN
  // NSIS treats /D as a raw final command-line argument.
  if (path.contains(QLatin1Char('*')) || path.contains(QLatin1Char('?')) || path.endsWith(QLatin1Char('.')))
    return I18n::tr("Invalid characters in path.");
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
    emit finished(false, I18n::tr("Installation files not found. Launch Setup from downloaded package."));
    return;
  }
#ifdef Q_OS_WIN
  QFile partialMarker(directory_ + QStringLiteral("/.vynx-qr-install"));
  const bool partialInstall = partialMarker.open(QIODevice::ReadOnly)
      && partialMarker.readAll() == QByteArray("VYNX QR Windows setup");
  partialMarker.close();
  if (QFileInfo::exists(directory_) && !QDir(directory_).isEmpty() &&
      !partialInstall && !(QFileInfo::exists(executable()) && QFileInfo::exists(directory_ + QStringLiteral("/Uninstall.exe")))) {
    emit finished(false, I18n::tr("Folder contains other files. Choose an empty folder or existing VYNX QR installation."));
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
      fail(I18n::tr("Folder contains other files. Choose an empty folder or existing VYNX QR installation.")); return;
    }
    QFile previousDesktop(desktop);
    if (previousDesktop.exists()) {
      if (!previousDesktop.open(QIODevice::ReadOnly) ||
          !previousDesktop.readAll().startsWith(QByteArray("# VYNX QR: ") + directory_.toUtf8() + '\n')) {
        fail(I18n::tr("VYNX QR shortcut belongs to another installation. Uninstall it first.")); return;
      }
    }
    if (!QDir().mkpath(directory_) || !QDir().mkpath(QFileInfo(desktop).absolutePath())) {
      fail(I18n::tr("Failed to create folder. Check permissions.")); return;
    }
    // Identify a partially completed installation so failures can be retried.
    if (!writeFile(directory_ + marker, ownership(directory_))) {
      fail(I18n::tr("Failed to write files to selected folder.")); return;
    }
    QFile source(payload_);
    QSaveFile target(executable());
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
      fail(I18n::tr("Failed to open files. Check permissions.")); return;
    }
    qint64 copied = 0;
    while (!source.atEnd()) {
      const auto block = source.read(1024 * 1024);
      if (block.isEmpty() || target.write(block) != block.size()) {
        fail(I18n::tr("Failed to copy application. Check free disk space.")); return;
      }
      copied += block.size();
      emit progress(static_cast<int>(copied * 90 / qMax<qint64>(source.size(), 1)));
    }
    target.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                         | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
    if (!target.commit()) {
      fail(I18n::tr("Failed to save application. Check free disk space.")); return;
    }
    const auto icon = directory_ + QStringLiteral("/icon.png");
    // Gio validates argv[0] before expanding desktop %% escapes. Keep the
    // user-selected directory in an argument so literal percent signs work.
    // A fixed relative command also avoids env treating '=' in a path as an
    // environment assignment.
    const auto entry = QByteArray("# VYNX QR: ") + directory_.toUtf8() + '\n' +
        QStringLiteral("[Desktop Entry]\nType=Application\nName=VYNX QR\nComment=Fast, private QR codes\nExec=/usr/bin/env --chdir=%1 ./vynx-qr.AppImage\nIcon=%2\nTerminal=false\nCategories=Utility;\nActions=Uninstall;\n\n[Desktop Action Uninstall]\nName=Uninstall VYNX QR\nExec=/usr/bin/env --chdir=%1 ./vynx-qr.AppImage --uninstall\n")
            .arg(desktopQuote(directory_), QString(icon).replace(QLatin1Char('\\'), QStringLiteral("\\\\"))).toUtf8();
    if (!QFile::copy(QStringLiteral(":/icons/vynx-qr.png"), icon) && !QFileInfo::exists(icon)) {
      fail(I18n::tr("Failed to save application icon.")); return;
    }
    if (!writeFile(directory_ + marker, ownership(directory_)) || !writeFile(desktop, entry)) {
      // Files remain identifiable for a retry; no unrelated directory is removed.
      fail(I18n::tr("Application copied, but failed to create shortcut. Check permissions and retry.")); return;
    }
    emit progress(100);
    emit finished(true, {});
  });
  worker_->start();
#endif
}

bool SetupBackend::uninstallLinux(const QString &directory, QString *error) {
  if (!owned(directory)) { *error = I18n::tr("This folder does not belong to VYNX QR installer."); return false; }
  QFile desktop(desktopPath());
  if (desktop.exists()) {
    if (!desktop.open(QIODevice::ReadOnly) ||
        !desktop.readAll().startsWith(QByteArray("# VYNX QR: ") + directory.toUtf8() + '\n')) {
      *error = I18n::tr("Shortcut belongs to another installation."); return false;
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
