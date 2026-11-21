#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QTextEdit;

class ScanResultDialog final : public QDialog {
  Q_OBJECT

public:
  explicit ScanResultDialog(const QString &text, QWidget *parent = nullptr);
  ~ScanResultDialog() override = default;

signals:
  void insertRequested(const QString &text);

private slots:
  void onCopyClicked();
  void onOpenUrlClicked();
  void onInsertClicked();

private:
  QString text_;
  QTextEdit *textView_{nullptr};
  QPushButton *copyButton_{nullptr};
  QPushButton *openUrlButton_{nullptr};
  QPushButton *insertButton_{nullptr};
};
