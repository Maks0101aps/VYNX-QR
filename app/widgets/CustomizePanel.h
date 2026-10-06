#pragma once

#include <QColor>
#include <QWidget>

#include "vynx-qr-bridge/src/lib.rs.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

/// Everything about how the code looks.
///
/// A panel inside the window rather than a dialog, so changing a colour shows its
/// effect immediately instead of behind an OK button. Each control edits one
/// field of {@link RenderOptions} and nothing else; the engine still decides what
/// is drawable and raises error correction when a logo needs it.
class CustomizePanel final : public QWidget {
  Q_OBJECT

public:
  explicit CustomizePanel(QWidget *parent = nullptr);

  /// The current style, including any logo.
  [[nodiscard]] vynx::RenderOptions options(std::uint32_t sizePx) const;

  /// Start from the engine's defaults, discarding local changes.
  void reset();

  /// Adopt the loaded preferences, so the panel reflects them on open.
  void applySettings(const vynx::Settings &settings);

  /// Attach a validated logo and reflect it in the panel.
  void setLogo(const vynx::Logo &logo);

  /// Detach the logo.
  void removeLogo();

  /// True when a logo is attached.
  [[nodiscard]] bool hasLogo() const;

  /// The size the export should use.
  [[nodiscard]] std::uint32_t exportSize() const;

  /// The error correction the user asked for, before the engine adjusts it.
  [[nodiscard]] vynx::EcLevel errorCorrection() const;

signals:
  /// A control changed, so the code should be re-rendered.
  void edited();

  /// The user asked to add a logo.
  void addLogoRequested();

  /// The user asked to remove the logo.
  void removeLogoRequested();

private:
  QWidget *buildStyleRow();
  QWidget *buildColourRow(const QString &label, QLineEdit *&box, QPushButton *&swatch);
  QWidget *buildLogoRow();

  /// Push the current widget values into `options`.
  void writeInto(vynx::RenderOptions &options) const;

  /// Keep a swatch and its last valid colour together.
  void updateSwatch(QPushButton *swatch, const QString &hex);

  QComboBox *moduleStyle_ = nullptr;
  QComboBox *errorCorrection_ = nullptr;
  QComboBox *exportSize_ = nullptr;
  QComboBox *quietZone_ = nullptr;
  QLineEdit *foreground_ = nullptr;
  QLineEdit *background_ = nullptr;
  QPushButton *foregroundSwatch_ = nullptr;
  QPushButton *backgroundSwatch_ = nullptr;
  QSlider *logoRatio_ = nullptr;
  QPushButton *removeLogo_ = nullptr;
  QLabel *logoName_ = nullptr;
  QLabel *logoHint_ = nullptr;

  vynx::Logo logo_;
  bool hasLogo_ = false;
};
