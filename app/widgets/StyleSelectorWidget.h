#pragma once

#include <QWidget>

#include "vynx-qr-bridge/src/lib.rs.h"

class QButtonGroup;
class QToolButton;

/// Visual card selector for QR code module styles: Classic (Square), Rounded, Dots.
class StyleSelectorWidget final : public QWidget {
  Q_OBJECT

public:
  explicit StyleSelectorWidget(QWidget *parent = nullptr);

  [[nodiscard]] vynx::ModuleStyle selectedStyle() const;
  void setStyle(vynx::ModuleStyle style);
  void retranslateUi();

signals:
  void styleChanged(vynx::ModuleStyle style);

private:
  void updateIcons();

  QButtonGroup *group_ = nullptr;
  QToolButton *classicBtn_ = nullptr;
  QToolButton *roundedBtn_ = nullptr;
  QToolButton *dotsBtn_ = nullptr;
};
