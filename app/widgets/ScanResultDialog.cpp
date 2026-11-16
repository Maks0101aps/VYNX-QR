#include "ScanResultDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Theme.h"
#include "platform/PlatformIntegration.h"

ScanResultDialog::ScanResultDialog(const QString &text, QWidget *parent)
    : QDialog(parent), text_(text) {
  setWindowTitle(QStringLiteral("Распознанный QR-код"));
  setModal(true);
  resize(500, 320);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                              Tokens::SpaceXLarge, Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceMedium);

  auto *headline = new QLabel(QStringLiteral("QR-код успешно распознан"));
  headline->setProperty("role", QStringLiteral("heading"));
  layout->addWidget(headline);

  const bool isUrl = text.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
                     text.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);

  auto *kindLabel = new QLabel(isUrl ? QStringLiteral("Ссылка:") : QStringLiteral("Содержимое:"));
  kindLabel->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(kindLabel);

  textView_ = new QTextEdit(this);
  textView_->setReadOnly(true);
  textView_->setPlainText(text);
  layout->addWidget(textView_, 1);

  auto *buttonBox = new QHBoxLayout();

  copyButton_ = new QPushButton(QStringLiteral("Скопировать"), this);
  connect(copyButton_, &QPushButton::clicked, this, &ScanResultDialog::onCopyClicked);
  buttonBox->addWidget(copyButton_);

  if (isUrl) {
    openUrlButton_ = new QPushButton(QStringLiteral("Открыть в браузере"), this);
    connect(openUrlButton_, &QPushButton::clicked, this, &ScanResultDialog::onOpenUrlClicked);
    buttonBox->addWidget(openUrlButton_);
  }

  buttonBox->addStretch(1);

  insertButton_ = new QPushButton(QStringLiteral("Вставить в генератор"), this);
  insertButton_->setProperty("accent", true);
  connect(insertButton_, &QPushButton::clicked, this, &ScanResultDialog::onInsertClicked);
  buttonBox->addWidget(insertButton_);

  auto *closeButton = new QPushButton(QStringLiteral("Закрыть"), this);
  closeButton->setProperty("flat", true);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
  buttonBox->addWidget(closeButton);

  layout->addLayout(buttonBox);
}

void ScanResultDialog::onCopyClicked() {
  QApplication::clipboard()->setText(text_);
  copyButton_->setText(QStringLiteral("Скопировано!"));
}

void ScanResultDialog::onOpenUrlClicked() {
  PlatformIntegration::openUrl(text_);
}

void ScanResultDialog::onInsertClicked() {
  emit insertRequested(text_);
  accept();
}
