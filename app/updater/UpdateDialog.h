#pragma once

#include <QDialog>
#include "AutoUpdater.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QTextEdit;

class UpdateDialog final : public QDialog {
  Q_OBJECT

public:
  UpdateDialog(const UpdateInfo &info, AutoUpdater *updater, QWidget *parent = nullptr);
  ~UpdateDialog() override = default;

private slots:
  void onUpdateClicked();
  void onProgress(qint64 received, qint64 total);
  void onFinished(const QString &filePath);
  void onError(const QString &errorMsg);

private:
  UpdateInfo info_;
  AutoUpdater *updater_{nullptr};

  QLabel *statusLabel_{nullptr};
  QProgressBar *progressBar_{nullptr};
  QPushButton *updateButton_{nullptr};
  QPushButton *cancelButton_{nullptr};
};
