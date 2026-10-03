#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

/// The start-up clipboard offer.
///
/// Shown only when there is something in the clipboard and the user has not
/// asked for it to be used automatically. Nothing is inserted into the input
/// until they press the button, because silently replacing what the user was
/// about to type is worse than a missed opportunity.
class ClipboardSuggestion final : public QWidget {
  Q_OBJECT

public:
  explicit ClipboardSuggestion(QWidget *parent = nullptr);

  /// Show the offer for `text`, or hide when there is nothing worth offering.
  void offer(const QString &text);

  void clear();

signals:
  /// The user accepted the clipboard content.
  void accepted(const QString &text);

  /// The user dismissed the offer.
  void dismissed();

private:
  QLabel *preview_ = nullptr;
  QPushButton *use_ = nullptr;
  QPushButton *dismiss_ = nullptr;
  QString text_;
};