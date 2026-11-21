#pragma once

#include <QDialog>

#include "vynx-qr-bridge/src/lib.rs.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// Preferences, edited in place.
///
/// Every control writes straight through to the engine's settings file, so there
/// is no Save button and no state to lose by closing the window. The file is the
/// same one the previous Tauri build wrote, so preferences survive the migration.
class SettingsDialog final : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = nullptr);

  /// The settings as edited so far.
  [[nodiscard]] vynx::Settings settings() const;

signals:
  /// Any control changed. The window applies the theme and the panel immediately.
  void settingsChanged(const vynx::Settings &settings);

private:
  void buildLanguage(QVBoxLayout *layout);
  void buildAppearance(QVBoxLayout *layout);
  void buildBehaviour(QVBoxLayout *layout);
  void buildExport(QVBoxLayout *layout);
  void retranslateUi();

  /// Keep the clipboard options consistent with each other.
  ///
  /// Automatically using the clipboard while never looking at it is a
  /// contradiction, so turning the check off turns the automatic use off too.
  void updateClipboardDependency();

  vynx::Settings settings_;

  QLabel *languageTitle_ = nullptr;
  QComboBox *language_ = nullptr;
  QLabel *appearanceTitle_ = nullptr;
  QComboBox *theme_ = nullptr;
  QCheckBox *useAccent_ = nullptr;
  QLabel *behaviourTitle_ = nullptr;
  QCheckBox *clipboardCheck_ = nullptr;
  QCheckBox *autoPaste_ = nullptr;
  QLabel *exportTitle_ = nullptr;
  QComboBox *format_ = nullptr;
  QComboBox *size_ = nullptr;
  QComboBox *errorCorrection_ = nullptr;
  QComboBox *moduleStyle_ = nullptr;
  QPushButton *closeButton_ = nullptr;
};