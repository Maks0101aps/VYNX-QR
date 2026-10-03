#include "ClipboardSuggestion.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "Theme.h"

namespace {

/// Long clipboard content is trimmed for display but offered in full.
QString elide(const QString &text) {
  const QString collapsed = QString(text).replace(QStringLiteral("\n"), QStringLiteral(" "));
  return collapsed.length() > 90 ? collapsed.left(89) + QStringLiteral("…") : collapsed;
}

} // namespace

ClipboardSuggestion::ClipboardSuggestion(QWidget *parent) : QWidget(parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  auto *label = new QLabel(QStringLiteral("Found in your clipboard"));
  label->setProperty("role", QStringLiteral("caption"));
  label->setAccessibleName(QStringLiteral("Clipboard suggestion"));
  layout->addWidget(label);

  preview_ = new QLabel(this);
  preview_->setProperty("role", QStringLiteral("caption"));
  preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(preview_, 1);

  use_ = new QPushButton(QStringLiteral("Use clipboard"));
  use_->setCursor(Qt::PointingHandCursor);
  use_->setAccessibleName(QStringLiteral("Use the clipboard content"));
  layout->addWidget(use_);

  dismiss_ = new QPushButton(QStringLiteral("×"));
  dismiss_->setProperty("flat", true);
  dismiss_->setCursor(Qt::PointingHandCursor);
  dismiss_->setAccessibleName(QStringLiteral("Dismiss the clipboard suggestion"));
  layout->addWidget(dismiss_);

  connect(use_, &QPushButton::clicked, this, [this] {
    const QString value = text_;
    clear();
    emit accepted(value);
  });
  connect(dismiss_, &QPushButton::clicked, this, [this] {
    clear();
    emit dismissed();
  });

  hide();
}

void ClipboardSuggestion::offer(const QString &text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    clear();
    return;
  }
  text_ = trimmed;
  preview_->setText(elide(trimmed));
  show();
}

void ClipboardSuggestion::clear() {
  text_.clear();
  preview_->clear();
  hide();
}