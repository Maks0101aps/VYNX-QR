#pragma once

#include <QObject>
#include <QString>
#include <functional>

struct UpdateInfo {
  QString version;
  QString tagName;
  QString releaseNotes;
  QString releaseUrl;
  QString downloadUrl;
  QString assetName;
  qint64 assetSize{0};
};

class AutoUpdater final : public QObject {
  Q_OBJECT

public:
  explicit AutoUpdater(QObject *parent = nullptr);
  ~AutoUpdater() override;

  /// Check for new releases on GitHub in a background thread.
  /// If silent is true, no dialog is shown if up to date or on network error.
  void checkForUpdates(bool silent = true);

  /// Download the asset and trigger silent installation with application relaunch.
  void downloadAndInstall(const UpdateInfo &info);

  /// Compare two semantic version strings (e.g. "v1.1.4" vs "1.1.3").
  static bool isNewerVersion(const QString &remote, const QString &current);

  /// Run the silent installer and restart the application.
  static void installAndRestart(const QString &installerPath);

signals:
  void updateAvailable(const UpdateInfo &info);
  void noUpdateAvailable();
  void checkFailed(const QString &errorMessage);

  void downloadProgress(qint64 received, qint64 total);
  void downloadFinished(const QString &filePath);
  void downloadFailed(const QString &errorMessage);

private:
  bool checking_{false};
  bool downloading_{false};
};
