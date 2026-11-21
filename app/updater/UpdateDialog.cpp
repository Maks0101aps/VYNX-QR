#include "UpdateDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Theme.h"
#include "i18n/I18n.h"
#include "platform/PlatformIntegration.h"

UpdateDialog::UpdateDialog(const UpdateInfo &info, AutoUpdater *updater, QWidget *parent)
    : QDialog(parent), info_(info), updater_(updater) {
  setWindowTitle(I18n::tr("VYNX QR Update"));
  setModal(true);
  resize(460, 280);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                              Tokens::SpaceXLarge, Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceMedium);

  auto *headline = new QLabel(I18n::tr("New version available!"));
  headline->setProperty("role", QStringLiteral("heading"));
  layout->addWidget(headline);

  auto *infoLabel = new QLabel(I18n::tr("A new version <b>%1</b> is available (current: %2).<br>"
                                        "Would you like to download and install the update?")
                                   .arg(info.version, PlatformIntegration::versionString()));
  infoLabel->setTextFormat(Qt::RichText);
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  if (!info.releaseNotes.isEmpty()) {
    auto *notesLabel = new QLabel(I18n::tr("What's new:"));
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

  cancelButton_ = new QPushButton(I18n::tr("Later"), this);
  cancelButton_->setProperty("flat", true);
  connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
  buttonBox->addWidget(cancelButton_);

  updateButton_ = new QPushButton(I18n::tr("Update"), this);
  updateButton_->setProperty("accent", true);
  connect(updateButton_, &QPushButton::clicked, this, &UpdateDialog::onUpdateClicked);
  buttonBox->addWidget(updateButton_);

  layout->addLayout(buttonBox);
}

void UpdateDialog::onUpdateClicked() {
  updateButton_->setEnabled(false);
  updateButton_->setVisible(false);
  cancelButton_->setText(I18n::tr("Cancel"));
  statusLabel_->setText(I18n::tr("Downloading update..."));
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
    statusLabel_->setText(I18n::tr("Downloading update: %1 MB of %2 MB (%3%)")
                              .arg(QString::number(mbReceived, 'f', 1),
                                   QString::number(mbTotal, 'f', 1),
                                   QString::number(percent)));
  } else {
    progressBar_->setRange(0, 0); // Indeterminate
    const double mbReceived = received / (1024.0 * 1024.0);
    statusLabel_->setText(I18n::tr("Downloading update: %1 MB")
                              .arg(QString::number(mbReceived, 'f', 1)));
  }
}

void UpdateDialog::onFinished(const QString &filePath) {
  progressBar_->setRange(0, 100);
  progressBar_->setValue(100);
  statusLabel_->setText(I18n::tr("Ready to install. VYNX QR will restart."));
  cancelButton_->setEnabled(false);
  AutoUpdater::installAndRestart(filePath);
}

void UpdateDialog::onError(const QString &errorMsg) {
  progressBar_->setVisible(false);
  statusLabel_->setText(I18n::tr("Failed to download update: %1").arg(errorMsg));
  cancelButton_->setText(I18n::tr("Close"));
  updateButton_->setText(I18n::tr("Open in browser"));
  updateButton_->setVisible(true);
  updateButton_->setEnabled(true);
  disconnect(updateButton_, &QPushButton::clicked, this, &UpdateDialog::onUpdateClicked);
  connect(updateButton_, &QPushButton::clicked, this, [this] {
    const QString targetUrl = info_.releaseUrl.isEmpty() ? PlatformIntegration::repositoryUrl() : info_.releaseUrl;
    PlatformIntegration::openUrl(targetUrl);
    accept();
  });
}
