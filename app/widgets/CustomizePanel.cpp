#include "CustomizePanel.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "Bridge.h"
#include "Theme.h"

namespace {

QWidget *sectionLabel(const QString &text) {
  auto *label = new QLabel(text);
  label->setProperty("role", QStringLiteral("caption"));
  return label;
}

} // namespace

CustomizePanel::CustomizePanel(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);

  layout->addWidget(sectionLabel(QStringLiteral("STYLE")));
  layout->addWidget(buildStyleRow());
  layout->addWidget(buildColourRow(QStringLiteral("Foreground"), foreground_,
                                    foregroundSwatch_));
  layout->addWidget(buildColourRow(QStringLiteral("Background"), background_,
                                    backgroundSwatch_));
  layout->addWidget(sectionLabel(QStringLiteral("ERROR CORRECTION")));
  layout->addWidget(buildStyleRow());
  layout->addWidget(sectionLabel(QStringLiteral("EXPORT")));
  layout->addWidget(buildLogoRow());
  layout->addStretch(1);

  updateSwatch(foreground_->text());
  updateSwatch(background_->text());
}

QWidget *CustomizePanel::buildStyleRow() {
  auto *row = new QWidget;
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  moduleStyle_ = new QComboBox;
  moduleStyle_->addItems({QStringLiteral("Square"), QStringLiteral("Rounded")});
  moduleStyle_->setAccessibleName(QStringLiteral("Module shape"));
  moduleStyle_->setToolTip(QStringLiteral("How the QR modules are drawn."));
  layout->addWidget(moduleStyle_, 1);

  errorCorrection_ = new QComboBox;
  errorCorrection_->addItems({QStringLiteral("Low"), QStringLiteral("Medium"),
                              QStringLiteral("Quartile"), QStringLiteral("High")});
  errorCorrection_->setAccessibleName(QStringLiteral("Error correction"));
  errorCorrection_->setToolTip(
      QStringLiteral("Extra redundancy. A logo raises this to High automatically."));
  layout->addWidget(errorCorrection_, 1);

  quietZone_ = new QComboBox;
  for (int zone : {1, 2, 3, 4, 6, 8}) {
    quietZone_->addItem(QString::number(zone), zone);
  }
  quietZone_->setAccessibleName(QStringLiteral("Quiet zone in modules"));
  quietZone_->setToolTip(QStringLiteral("The margin around the code. At least one is required."));
  layout->addWidget(quietZone_, 1);

  exportSize_ = new QComboBox;
  for (std::uint32_t size : {256u, 512u, 1024u, 2048u}) {
    exportSize_->addItem(QStringLiteral("%1 px").arg(size), size);
  }
  exportSize_->setAccessibleName(QStringLiteral("Export size"));
  exportSize_->setToolTip(QStringLiteral("Used when saving, not when previewing."));
  layout->addWidget(exportSize_, 1);

  connect(moduleStyle_, &QComboBox::currentIndexChanged, this, &CustomizePanel::edited);
  connect(errorCorrection_, &QComboBox::currentIndexChanged, this, &CustomizePanel::edited);
  connect(quietZone_, &QComboBox::currentIndexChanged, this, &CustomizePanel::edited);
  connect(exportSize_, &QComboBox::currentIndexChanged, this, &CustomizePanel::edited);
  return row;
}

QWidget *CustomizePanel::buildColourRow(const QString &label, QLineEdit *&box,
                                        QPushButton *&swatch) {
  auto *row = new QWidget;
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  swatch = new QPushButton;
  swatch->setFixedSize(28, 28);
  swatch->setCursor(Qt::PointingHandCursor);
  swatch->setAccessibleName(QStringLiteral("Choose %1 colour").arg(label));
  layout->addWidget(swatch);

  box = new QLineEdit;
  box->setAccessibleName(QStringLiteral("%1 HEX value").arg(label));
  layout->addWidget(box, 1);

  connect(box, &QLineEdit::textEdited, this, [this, box, swatch] {
    // A value that is not a colour never reaches the engine: the swatch simply
    // stops following it, and the field is marked so the mistake is visible.
    const bool valid = vynxq::isHexColour(box->text());
    box->setProperty("invalid", valid ? QStringLiteral("false") : QStringLiteral("true"));
    box->style()->unpolish(box);
    box->style()->polish(box);
    if (valid) {
      updateSwatch(box->text());
      emit edited();
    }
  });

  connect(swatch, &QPushButton::clicked, this, [this, box, label] {
    const QColor chosen =
        QColorDialog::getColor(vynxq::parseHexColour(box->text()), this,
                               QStringLiteral("Choose %1").arg(label));
    if (!chosen.isValid()) {
      return;
    }
    box->setText(vynxq::toHex(chosen));
    box->setProperty("invalid", QStringLiteral("false"));
    box->style()->unpolish(box);
    box->style()->polish(box);
    updateSwatch(box->text());
    emit edited();
  });

  return row;
}

QWidget *CustomizePanel::buildLogoRow() {
  auto *row = new QWidget;
  auto *layout = new QVBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  auto *buttons = new QHBoxLayout;
  buttons->setSpacing(Tokens::SpaceSmall);

  auto *add = new QPushButton(QStringLiteral("Add logo…"));
  add->setCursor(Qt::PointingHandCursor);
  add->setAccessibleName(QStringLiteral("Add a logo image"));
  buttons->addWidget(add);

  removeLogo_ = new QPushButton(QStringLiteral("Remove"));
  removeLogo_->setCursor(Qt::PointingHandCursor);
  removeLogo_->setAccessibleName(QStringLiteral("Remove the logo"));
  removeLogo_->setEnabled(false);
  buttons->addWidget(removeLogo_);
  buttons->addStretch(1);
  layout->addLayout(buttons);

  logoName_ = new QLabel(QStringLiteral("No logo"));
  logoName_->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(logoName_);

  logoHint_ = new QLabel(QStringLiteral("PNG, JPEG or WebP, up to 2 MB."));
  logoHint_->setProperty("role", QStringLiteral("caption"));
  logoHint_->setWordWrap(true);
  layout->addWidget(logoHint_);

  logoRatio_ = new QSlider(Qt::Horizontal);
  logoRatio_->setRange(5, 30);
  logoRatio_->setValue(20);
  logoRatio_->setAccessibleName(QStringLiteral("Logo size percentage"));
  logoRatio_->setEnabled(false);
  layout->addWidget(logoRatio_);

  connect(add, &QPushButton::clicked, this, &CustomizePanel::addLogoRequested);
  connect(removeLogo_, &QPushButton::clicked, this, &CustomizePanel::removeLogoRequested);
  connect(logoRatio_, &QSlider::valueChanged, this, &CustomizePanel::edited);
  return row;
}

void CustomizePanel::updateSwatch(const QString &hex) {
  const QColor colour = vynxq::parseHexColour(hex);
  const QString stylesheet = colour.isValid()
                                 ? QStringLiteral("background: %1;").arg(vynxq::toHex(colour))
                                 : QStringLiteral("background: transparent;");
  const QColor border = colour.isValid() ? vynxq::parseHexColour(hex) : QColor(Qt::gray);
  for (QPushButton *swatch : {foregroundSwatch_, backgroundSwatch_}) {
    swatch->setStyleSheet(
        QStringLiteral("%1 border: 1px solid %2; border-radius: 4px;")
            .arg(stylesheet, border.name(QColor::HexRgb)));
  }
}

void CustomizePanel::setLogo(const vynx::Logo &logo) {
  logo_ = logo;
  hasLogo_ = !logo_.name.empty();
  logoName_->setText(hasLogo_ ? vynxq::toQString(logo_.name) : QStringLiteral("No logo"));
  removeLogo_->setEnabled(hasLogo_);
  logoRatio_->setEnabled(hasLogo_);
  logoHint_->setText(hasLogo_ ? QStringLiteral("Error correction is raised to High for the logo.")
                              : QStringLiteral("PNG, JPEG or WebP, up to 2 MB."));
  emit edited();
}

void CustomizePanel::removeLogo() { setLogo(vynx::Logo()); }

void CustomizePanel::writeInto(vynx::RenderOptions &options) const {
  options.module_style =
      moduleStyle_->currentIndex() == 1 ? vynx::ModuleStyle::Rounded : vynx::ModuleStyle::Square;
  options.ec_level = errorCorrection() == vynx::EcLevel::L    ? vynx::EcLevel::L
                      : errorCorrection() == vynx::EcLevel::Q  ? vynx::EcLevel::Q
                      : errorCorrection() == vynx::EcLevel::M  ? vynx::EcLevel::M
                                                                : vynx::EcLevel::H;
  options.quiet_zone = static_cast<std::uint32_t>(quietZone_->currentData().toUInt());
  options.logo_ratio = static_cast<float>(logoRatio_->value()) / 100.0f;

  // An invalid colour is simply not sent: the last good one stays in effect, so
  // a half typed hex never produces an unreadable code.
  if (vynxq::isHexColour(foreground_->text())) {
    options.foreground = vynxq::toRust(vynxq::toHex(vynxq::parseHexColour(foreground_->text())));
  }
  if (vynxq::isHexColour(background_->text())) {
    options.background =
        vynxq::toRust(vynxq::toHex(vynxq::parseHexColour(background_->text())));
  }
}

vynx::RenderOptions CustomizePanel::options(std::uint32_t sizePx) const {
  vynx::RenderOptions result = vynx::default_style();
  writeInto(result);
  result.size_px = sizePx;
  result.has_logo = hasLogo_;
  result.logo = logo_;
  return result;
}

vynx::EcLevel CustomizePanel::errorCorrection() const {
  switch (errorCorrection_->currentIndex()) {
  case 0: return vynx::EcLevel::L;
  case 2: return vynx::EcLevel::Q;
  case 3: return vynx::EcLevel::H;
  default: return vynx::EcLevel::M;
  }
}

std::uint32_t CustomizePanel::exportSize() const {
  return static_cast<std::uint32_t>(exportSize_->currentData().toUInt());
}

bool CustomizePanel::hasLogo() const { return hasLogo_; }

void CustomizePanel::applySettings(const vynx::Settings &settings) {
  moduleStyle_->setCurrentIndex(settings.default_module_style == vynx::ModuleStyle::Rounded ? 1 : 0);
  switch (settings.default_error_correction) {
  case vynx::EcLevel::L: errorCorrection_->setCurrentIndex(0); break;
  case vynx::EcLevel::Q: errorCorrection_->setCurrentIndex(2); break;
  case vynx::EcLevel::H: errorCorrection_->setCurrentIndex(3); break;
  default: errorCorrection_->setCurrentIndex(1); break;
  }
  exportSize_->setCurrentIndex(exportSize_->findData(static_cast<int>(settings.default_size)));
}

void CustomizePanel::reset() {
  moduleStyle_->setCurrentIndex(0);
  errorCorrection_->setCurrentIndex(1);
  quietZone_->setCurrentIndex(3);
  exportSize_->setCurrentIndex(2);
  logoRatio_->setValue(20);
  hasLogo_ = false;
  logo_ = vynx::Logo();
  logoName_->setText(QStringLiteral("No logo"));
  removeLogo_->setEnabled(false);
  logoRatio_->setEnabled(false);
  emit edited();
}