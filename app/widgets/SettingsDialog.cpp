#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "Bridge.h"
#include "Theme.h"

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
  settings_ = vynx::load_settings();

  setWindowTitle(QStringLiteral("Settings"));
  setModal(true);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                              Tokens::SpaceXLarge, Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceLarge);

  buildAppearance(layout);
  buildBehaviour(layout);
  buildExport(layout);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this,
          [this] { emit settingsChanged(settings()); });
  layout->addWidget(buttons);

  sizeHint();
}

void SettingsDialog::buildAppearance(QVBoxLayout *layout) {
  auto *title = new QLabel(QStringLiteral("Appearance"));
  title->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(title);

  theme_ = new QComboBox;
  theme_->addItems({QStringLiteral("System"), QStringLiteral("Light"), QStringLiteral("Dark")});
  theme_->setCurrentIndex(settings_.theme == vynx::ThemeMode::Dark    ? 2
                          : settings_.theme == vynx::ThemeMode::Light ? 1
                                                                    : 0);
  theme_->setAccessibleName(QStringLiteral("Theme"));
  layout->addWidget(theme_);

  useAccent_ = new QCheckBox(QStringLiteral("Use system accent"));
  useAccent_->setChecked(settings_.use_windows_accent);
  useAccent_->setToolTip(QStringLiteral("Uses the system highlight colour, falling back to VYNX blue."));
  useAccent_->setAccessibleName(QStringLiteral("Use system accent"));
  layout->addWidget(useAccent_);

  connect(theme_, &QComboBox::currentIndexChanged, this, [this] {
    settings_.theme = theme_->currentIndex() == 2   ? vynx::ThemeMode::Dark
                      : theme_->currentIndex() == 1 ? vynx::ThemeMode::Light
                                                     : vynx::ThemeMode::System;
    emit settingsChanged(settings());
  });
  connect(useAccent_, &QCheckBox::toggled, this, [this](bool on) {
    settings_.use_windows_accent = on;
    emit settingsChanged(settings());
  });
}

void SettingsDialog::buildBehaviour(QVBoxLayout *layout) {
  auto *title = new QLabel(QStringLiteral("Behaviour"));
  title->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(title);

  clipboardCheck_ = new QCheckBox(QStringLiteral("Check the clipboard at launch"));
  clipboardCheck_->setChecked(settings_.clipboard_check);
  clipboardCheck_->setToolTip(
      QStringLiteral("Read once when the window opens. Never watched afterwards."));
  clipboardCheck_->setAccessibleName(QStringLiteral("Check the clipboard at launch"));
  layout->addWidget(clipboardCheck_);

  autoPaste_ = new QCheckBox(QStringLiteral("Use clipboard content immediately"));
  autoPaste_->setChecked(settings_.auto_paste);
  autoPaste_->setToolTip(
      QStringLiteral("Off means you are offered the clipboard content instead."));
  autoPaste_->setAccessibleName(QStringLiteral("Use clipboard content immediately"));
  layout->addWidget(autoPaste_);

  connect(clipboardCheck_, &QCheckBox::toggled, this, [this](bool on) {
    settings_.clipboard_check = on;
    updateClipboardDependency();
    emit settingsChanged(settings());
  });
  connect(autoPaste_, &QCheckBox::toggled, this, [this](bool on) {
    settings_.auto_paste = on;
    emit settingsChanged(settings());
  });

  updateClipboardDependency();
}

void SettingsDialog::updateClipboardDependency() {
  // Automatically using clipboard content while never looking at the clipboard
  // is not a state that can be reached, so it cannot be left switched on.
  autoPaste_->setEnabled(settings_.clipboard_check);
  if (!settings_.clipboard_check) {
    autoPaste_->setChecked(false);
    settings_.auto_paste = false;
  }
}

void SettingsDialog::buildExport(QVBoxLayout *layout) {
  auto *title = new QLabel(QStringLiteral("Export"));
  title->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(title);

  format_ = new QComboBox;
  format_->addItems({QStringLiteral("PNG image"), QStringLiteral("SVG vector")});
  format_->setCurrentIndex(settings_.default_format == vynx::ExportFormat::Svg ? 1 : 0);
  format_->setAccessibleName(QStringLiteral("Default export format"));
  format_->setToolTip(QStringLiteral("What Ctrl+S saves."));
  layout->addWidget(format_);

  size_ = new QComboBox;
  for (std::uint32_t value : {256u, 512u, 1024u, 2048u}) {
    size_->addItem(QStringLiteral("%1 px").arg(value), value);
  }
  size_->setCurrentIndex(size_->findData(static_cast<int>(settings_.default_size)));
  size_->setAccessibleName(QStringLiteral("Default export size"));
  layout->addWidget(size_);

  errorCorrection_ = new QComboBox;
  errorCorrection_->addItems({QStringLiteral("Low"), QStringLiteral("Medium"),
                              QStringLiteral("Quartile"), QStringLiteral("High")});
  errorCorrection_->setAccessibleName(QStringLiteral("Default error correction"));
  switch (settings_.default_error_correction) {
  case vynx::EcLevel::L: errorCorrection_->setCurrentIndex(0); break;
  case vynx::EcLevel::Q: errorCorrection_->setCurrentIndex(2); break;
  case vynx::EcLevel::H: errorCorrection_->setCurrentIndex(3); break;
  default: errorCorrection_->setCurrentIndex(1); break;
  }
  layout->addWidget(errorCorrection_);

  moduleStyle_ = new QComboBox;
  moduleStyle_->addItems({QStringLiteral("Square"), QStringLiteral("Rounded")});
  moduleStyle_->setCurrentIndex(settings_.default_module_style == vynx::ModuleStyle::Rounded ? 1 : 0);
  moduleStyle_->setAccessibleName(QStringLiteral("Default module shape"));
  layout->addWidget(moduleStyle_);

  connect(format_, &QComboBox::currentIndexChanged, this, [this](int index) {
    settings_.default_format = index == 1 ? vynx::ExportFormat::Svg : vynx::ExportFormat::Png;
    emit settingsChanged(settings());
  });
  connect(size_, &QComboBox::currentIndexChanged, this, [this](int index) {
    settings_.default_size = static_cast<std::uint32_t>(size_->itemData(index).toUInt());
    emit settingsChanged(settings());
  });
  connect(errorCorrection_, &QComboBox::currentIndexChanged, this, [this](int index) {
    settings_.default_error_correction = index == 0   ? vynx::EcLevel::L
                                         : index == 2 ? vynx::EcLevel::Q
                                         : index == 3 ? vynx::EcLevel::H
                                                      : vynx::EcLevel::M;
    emit settingsChanged(settings());
  });
  connect(moduleStyle_, &QComboBox::currentIndexChanged, this, [this](int index) {
    settings_.default_module_style =
        index == 1 ? vynx::ModuleStyle::Rounded : vynx::ModuleStyle::Square;
    emit settingsChanged(settings());
  });
}

vynx::Settings SettingsDialog::settings() const { return settings_; }