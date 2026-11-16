#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "Theme.h"
#include "platform/PlatformIntegration.h"
#include "updater/AutoUpdater.h"

AboutDialog::AboutDialog(QWidget *parent, AutoUpdater *updater)
    : QDialog(parent), updater_(updater) {
  setWindowTitle(QStringLiteral("About VYNX QR"));
  setModal(true);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                              Tokens::SpaceXLarge, Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceSmall);

  auto *name = new QLabel(QStringLiteral("VYNX QR"));
  name->setProperty("role", QStringLiteral("heading"));
  layout->addWidget(name);

  auto *version = new QLabel(QStringLiteral("Version %1 · %2").arg(PlatformIntegration::versionString(), PlatformIntegration::platformName()));
  version->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(version);

  auto *summary = new QLabel(QStringLiteral("Fast, private QR codes."));
  layout->addWidget(summary);

  auto *privacy = new QLabel(QStringLiteral("100% local. No telemetry, no account,\nno history."));
  privacy->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(privacy);
  auto *qtNotice = new QLabel(PlatformIntegration::qtLicenseNotice());
  qtNotice->setWordWrap(true);
  layout->addWidget(qtNotice);

  layout->addSpacing(Tokens::SpaceSmall);
  addLink(QStringLiteral("GitHub"), PlatformIntegration::repositoryUrl());
  addLink(QStringLiteral("Licence"), QStringLiteral("%1/blob/main/LICENSE")
                                       .arg(PlatformIntegration::repositoryUrl()));

  if (updater_) {
    auto *checkUpdateBtn = new QPushButton(QStringLiteral("Проверить обновления"), this);
    checkUpdateBtn->setProperty("flat", true);
    checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    connect(checkUpdateBtn, &QPushButton::clicked, this, [this] {
      connect(updater_, &AutoUpdater::noUpdateAvailable, this, [this] {
        QMessageBox::information(this, QStringLiteral("VYNX QR"),
                                 QStringLiteral("У вас установлена последняя версия VYNX QR (%1).")
                                     .arg(PlatformIntegration::versionString()));
      }, Qt::SingleShotConnection);

      connect(updater_, &AutoUpdater::checkFailed, this, [this](const QString &err) {
        QMessageBox::warning(this, QStringLiteral("VYNX QR"),
                             QStringLiteral("Не удалось проверить обновления: %1").arg(err));
      }, Qt::SingleShotConnection);

      updater_->checkForUpdates(false);
    });
    layout->addWidget(checkUpdateBtn);
  }

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
  layout->addWidget(buttons);
}

void AboutDialog::addLink(const QString &text, const QString &url) {
  auto *button = new QPushButton(text);
  button->setProperty("flat", true);
  button->setCursor(Qt::PointingHandCursor);
  button->setAccessibleName(QStringLiteral("Open %1 in your browser").arg(text));
  connect(button, &QPushButton::clicked, this,
          [url] { PlatformIntegration::openUrl(url); });
  layout()->addWidget(button);
}
