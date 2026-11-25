#include "StyleSelectorWidget.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>

#include "Theme.h"
#include "i18n/I18n.h"

namespace {

QIcon createStyleIcon(vynx::ModuleStyle style, const QColor &color) {
  constexpr int size = 28;
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(color);
  painter.setPen(Qt::NoPen);

  constexpr int cellSize = 9;
  constexpr int p1 = 3;
  constexpr int p2 = 16;

  const QPoint positions[] = {
      QPoint(p1, p1),
      QPoint(p2, p1),
      QPoint(p1, p2),
      QPoint(p2, p2)
  };

  for (const auto &pos : positions) {
    if (style == vynx::ModuleStyle::Dots) {
      painter.drawEllipse(pos.x(), pos.y(), cellSize, cellSize);
    } else if (style == vynx::ModuleStyle::Rounded) {
      painter.drawRoundedRect(pos.x(), pos.y(), cellSize, cellSize, 3.0, 3.0);
    } else {
      painter.setRenderHint(QPainter::Antialiasing, false);
      painter.drawRect(pos.x(), pos.y(), cellSize, cellSize);
      painter.setRenderHint(QPainter::Antialiasing, true);
    }
  }

  return QIcon(pixmap);
}

} // namespace

StyleSelectorWidget::StyleSelectorWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  auto setupCard = [this, layout](QToolButton *&btn, const QString &objName, vynx::ModuleStyle style) {
    btn = new QToolButton(this);
    btn->setObjectName(objName);
    btn->setProperty("role", QStringLiteral("styleCard"));
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIconSize(QSize(24, 24));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btn->setMinimumHeight(44);

    group_->addButton(btn);
    layout->addWidget(btn);

    connect(btn, &QToolButton::toggled, this, [this, style](bool checked) {
      if (checked) {
        emit styleChanged(style);
      }
    });
  };

  setupCard(classicBtn_, QStringLiteral("style_classic"), vynx::ModuleStyle::Square);
  setupCard(roundedBtn_, QStringLiteral("style_rounded"), vynx::ModuleStyle::Rounded);
  setupCard(dotsBtn_, QStringLiteral("style_dots"), vynx::ModuleStyle::Dots);

  classicBtn_->setChecked(true);
  updateIcons();
  retranslateUi();
}

vynx::ModuleStyle StyleSelectorWidget::selectedStyle() const {
  if (dotsBtn_ && dotsBtn_->isChecked()) {
    return vynx::ModuleStyle::Dots;
  }
  if (roundedBtn_ && roundedBtn_->isChecked()) {
    return vynx::ModuleStyle::Rounded;
  }
  return vynx::ModuleStyle::Square;
}

void StyleSelectorWidget::setStyle(vynx::ModuleStyle style) {
  switch (style) {
  case vynx::ModuleStyle::Dots:
    dotsBtn_->setChecked(true);
    break;
  case vynx::ModuleStyle::Rounded:
    roundedBtn_->setChecked(true);
    break;
  case vynx::ModuleStyle::Square:
  default:
    classicBtn_->setChecked(true);
    break;
  }
}

void StyleSelectorWidget::updateIcons() {
  const QColor brandColor = vynxBlue();
  classicBtn_->setIcon(createStyleIcon(vynx::ModuleStyle::Square, brandColor));
  roundedBtn_->setIcon(createStyleIcon(vynx::ModuleStyle::Rounded, brandColor));
  dotsBtn_->setIcon(createStyleIcon(vynx::ModuleStyle::Dots, brandColor));
}

void StyleSelectorWidget::retranslateUi() {
  classicBtn_->setText(I18n::tr("Classic"));
  roundedBtn_->setText(I18n::tr("Rounded"));
  dotsBtn_->setText(I18n::tr("Dots"));
}
