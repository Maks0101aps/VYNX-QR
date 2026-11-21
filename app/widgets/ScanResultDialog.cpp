#include "ScanResultDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Theme.h"
#include "i18n/I18n.h"
#include "platform/PlatformIntegration.h"

ScanResultDialog::ScanResultDialog(const QString &text, QWidget *parent)
    : QDialog(parent), text_(text) {
  setWindowTitle(I18n::tr("Scanned QR Code"));
  setModal(true);
  resize(500, 320);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                              Tokens::SpaceXLarge, Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceMedium);

  auto *headline = new QLabel(I18n::tr("QR code scanned successfully"));
  headline->setProperty("role", QStringLiteral("heading"));
  layout->addWidget(headline);

  const bool isUrl = text.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
                     text.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);

  auto *kindLabel = new QLabel(isUrl ? I18n::tr("Link:") : I18n::tr("Content:"));
  kindLabel->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(kindLabel);

  textView_ = new QTextEdit(this);
  textView_->setReadOnly(true);
  textView_->setPlainText(text);
  layout->addWidget(textView_, 1);

  auto *buttonBox = new QHBoxLayout();

  copyButton_ = new QPushButton(I18n::tr("Copy"), this);
  connect(copyButton_, &QPushButton::clicked, this, &ScanResultDialog::onCopyClicked);
  buttonBox->addWidget(copyButton_);

  if (isUrl) {
    openUrlButton_ = new QPushButton(I18n::tr("Open in browser"), this);
    connect(openUrlButton_, &QPushButton::clicked, this, &ScanResultDialog::onOpenUrlClicked);
    buttonBox->addWidget(openUrlButton_);
  }

  buttonBox->addStretch(1);

  insertButton_ = new QPushButton(I18n::tr("Insert into generator"), this);
  insertButton_->setProperty("accent", true);
  connect(insertButton_, &QPushButton::clicked, this, &ScanResultDialog::onInsertClicked);
  buttonBox->addWidget(insertButton_);

  auto *closeButton = new QPushButton(I18n::tr("Close"), this);
  closeButton->setProperty("flat", true);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonBox->addWidget(closeButton);

  layout->addLayout(buttonBox);
}

void ScanResultDialog::onCopyClicked() {
  QApplication::clipboard()->setText(text_);
  copyButton_->setText(I18n::tr("Copied!"));
}

void ScanResultDialog::onOpenUrlClicked() {
  PlatformIntegration::openUrl(text_);
}

void ScanResultDialog::onInsertClicked() {
  emit insertRequested(text_);
  accept();
}
