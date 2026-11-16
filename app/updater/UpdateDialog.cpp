#include "UpdateDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Theme.h"
#include "platform/PlatformIntegration.h"

UpdateDialog::UpdateDialog(const UpdateInfo &info, AutoUpdater *updater, QWidget *parent)
    : QDialog(parent), info_(info), updater_(updater) {
  setWindowTitle(QStringLiteral("Обновление VYNX QR"));
  setModal(true);
  resize(460, 280);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                              Tokens::SpaceXLarge, Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceMedium);

  auto *headline = new QLabel(QStringLiteral("Доступна новая версия!"));
  headline->setProperty("role", QStringLiteral("heading"));
  layout->addWidget(headline);

  auto *infoLabel = new QLabel(QStringLiteral("Доступна новая версия <b>%1</b> (текущая: %2).<br>"
                                              "Хотите скачать и установить обновление?")
                                   .arg(info.version, PlatformIntegration::versionString()));
  infoLabel->setTextFormat(Qt::RichText);
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  if (!info.releaseNotes.isEmpty()) {
    auto *notesLabel = new QLabel(QStringLiteral("Что нового:"));
    notesLabel->setProperty("role", QStringLiteral("caption"));
    layout->addWidget(notesLabel);

    auto *notesView = new QTextEdit(this);
    notesView->setReadOnly(true);
    notesView->setPlainText(info.releaseNotes);
    notesView->setMaximumHeight(100);
    layout->addWidget(notesView);
  }

  statusLabel_ = new QLabel(this);
  statusLabel_->setProperty("role", QStringLiteral("caption"));
  statusLabel_->setVisible(false);
  layout->addWidget(statusLabel_);

  progressBar_ = new QProgressBar(this);
  progressBar_->setRange(0, 100);
  progressBar_->setValue(0);
  progressBar_->setVisible(false);
  layout->addWidget(progressBar_);

  auto *buttonBox = new QHBoxLayout();
  buttonBox->addStretch(1);

  cancelButton_ = new QPushButton(QStringLiteral("Позже"), this);
  cancelButton_->setProperty("flat", true);
  connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
  buttonBox->addWidget(cancelButton_);

  updateButton_ = new QPushButton(QStringLiteral("Обновить"), this);
  updateButton_->setProperty("accent", true);
  connect(updateButton_, &QPushButton::clicked, this, &UpdateDialog::onUpdateClicked);
  buttonBox->addWidget(updateButton_);

  layout->addLayout(buttonBox);
}

void UpdateDialog::onUpdateClicked() {
  updateButton_->setEnabled(false);
  updateButton_->setVisible(false);
  cancelButton_->setText(QStringLiteral("Отмена"));
  statusLabel_->setText(QStringLiteral("Загрузка обновления..."));
  statusLabel_->setVisible(true);
  progressBar_->setVisible(true);

  connect(updater_, &AutoUpdater::downloadProgress, this, &UpdateDialog::onProgress);
  connect(updater_, &AutoUpdater::downloadFinished, this, &UpdateDialog::onFinished);
  connect(updater_, &AutoUpdater::downloadFailed, this, &UpdateDialog::onError);

  updater_->downloadAndInstall(info_);
}

void UpdateDialog::onProgress(qint64 received, qint64 total) {
  if (total > 0) {
    const int percent = static_cast<int>((received * 100) / total);
    progressBar_->setValue(percent);
    const double mbReceived = received / (1024.0 * 1024.0);
    const double mbTotal = total / (1024.0 * 1024.0);
    statusLabel_->setText(QStringLiteral("Загрузка обновления: %1 МБ из %2 МБ (%3%)")
                              .arg(QString::number(mbReceived, 'f', 1),
                                   QString::number(mbTotal, 'f', 1),
                                   QString::number(percent)));
  } else {
    progressBar_->setRange(0, 0); // Indeterminate
    const double mbReceived = received / (1024.0 * 1024.0);
    statusLabel_->setText(QStringLiteral("Загрузка обновления: %1 МБ")
                              .arg(QString::number(mbReceived, 'f', 1)));
  }
}

void UpdateDialog::onFinished(const QString &filePath) {
  progressBar_->setRange(0, 100);
  progressBar_->setValue(100);
  statusLabel_->setText(QStringLiteral("Установка обновления и перезапуск программы..."));
  cancelButton_->setEnabled(false);
  AutoUpdater::installAndRestart(filePath);
}

void UpdateDialog::onError(const QString &errorMsg) {
  progressBar_->setVisible(false);
  statusLabel_->setText(QStringLiteral("Не удалось скачать обновление: %1").arg(errorMsg));
  cancelButton_->setText(QStringLiteral("Закрыть"));
  updateButton_->setText(QStringLiteral("Открыть в браузере"));
  updateButton_->setVisible(true);
  updateButton_->setEnabled(true);
  disconnect(updateButton_, &QPushButton::clicked, this, &UpdateDialog::onUpdateClicked);
  connect(updateButton_, &QPushButton::clicked, this, [this] {
    const QString targetUrl = info_.releaseUrl.isEmpty() ? PlatformIntegration::repositoryUrl() : info_.releaseUrl;
    PlatformIntegration::openUrl(targetUrl);
    accept();
  });
}
