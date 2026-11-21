#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

class ScreenSnipper final : public QWidget {
  Q_OBJECT

public:
  explicit ScreenSnipper(QWidget *parent = nullptr);
  ~ScreenSnipper() override = default;

  void startSnipping();

signals:
  void codeDecoded(const QString &text);
  void decodeFailed();
  void snippingCancelled();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QImage screenshot_;
  QPoint startPos_;
  QPoint currentPos_;
  bool selecting_{false};

  QRect selectionRect() const;
  void processSelection();
};
