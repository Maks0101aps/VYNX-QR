#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Theme.h"
#include "platform/PlatformIntegration.h"

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
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

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
  layout->addWidget(buttons);
}

void AboutDialog::addLink(const QString &text, const QString &url) {
  auto *button = new QPushButton(text);
  button->setProperty("flat", true);
  button->setCursor(Qt::PointingHandCursor);
  button->setAccessibleName(QStringLiteral("Open %1 in your browser").arg(text));
  // The shell opens it, so the application never opens a socket itself.
  connect(button, &QPushButton::clicked, this,
          [url] { PlatformIntegration::openUrl(url); });
  layout()->addWidget(button);
}
