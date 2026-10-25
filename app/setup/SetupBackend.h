#pragma once
#include <QObject>
#include <QProcess>
#include <QThread>

class SetupBackend : public QObject {
  Q_OBJECT
public:
  explicit SetupBackend(QString payload, QObject *parent = nullptr);
  ~SetupBackend() override;
  static QString defaultDirectory();
  static QString pathError(const QString &path);
  static bool uninstallLinux(const QString &directory, QString *error);
  void install(const QString &directory);
  QString executable() const;
signals:
  void progress(int percent);
  void finished(bool success, const QString &error);
private:
  QString payload_, directory_;
  QProcess process_;
  QThread *worker_ = nullptr;
};
