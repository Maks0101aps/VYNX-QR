#pragma once

#include <QDialog>

/// A small About box: what this is, what it does not do, and where it lives.
class AboutDialog final : public QDialog {
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr);

private:
  void addLink(const QString &text, const QString &url);
};