#pragma once

#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QString>
#include <QTimer>

#include <memory>

#include <rust/cxx.h>

#include "vynx-qr-bridge/src/lib.rs.h"

class QPushButton;
class QVBoxLayout;

/// The whole window.
///
/// It owns state and orchestrates; every visual piece is a plain widget. The QR
/// itself is never drawn here: the engine returns finished PNG bytes and this
/// class only decides what to show them in.
class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  /// The size the preview is rendered at.
  ///
  /// Deliberately not the export size. A 2048 pixel export re-rendered on every
  /// keystroke would cost memory and time for nothing, since the widget shows at
  /// most a few hundred pixels anyway.
  static constexpr std::uint32_t PreviewSize = 768;

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onInputChanged();
  void onInputSubmitted();
  void onCopy();
  void onSave();
  void onOpenSettings();
  void onClearInput();

private:
  /// Re-run detection and rendering for the current input.
  void refresh();

  /// Render at `size` and hand back finished PNG bytes.
  ///
  /// Returns an empty array when the engine refuses, after showing the reason.
  std::vector<std::uint8_t> renderPng(std::uint32_t size);

  /// Whether there is anything worth copying or saving.
  [[nodiscard]] bool hasCode() const;

  void showEmptyState();
  void showPreview(const QImage &image);
  void showError(const QString &message);
  void setVerification(vynx::VerifyStatus status);
  void updateChip();

  /// Copy the current code to the clipboard as a real image.
  void copyToClipboard();

  /// Ask for a destination and write the chosen format.
  void saveToDisk();

  /// Build the payload the engine should encode from the current input.
  [[nodiscard]] vynx::Payload currentPayload() const;

  vynx::RenderOptions currentOptions(std::uint32_t size) const;

  /// Debounces detection so a fast typist does not trigger a render per key.
  QTimer debounce_;

  QString inputText_;
  QString normalizationNotice_;
  QString detectedKind_;
  bool detected_ = false;
  bool useOriginalText_ = false;
  /// Bumped on every request; a result from an older generation is discarded, so
  /// typing quickly can never leave a stale code on screen.
  std::uint64_t generation_ = 0;
  /// The result currently on screen, used by copy and save.
  vynx::GenerateResult current_;

  vynx::Settings settings_;

  QLineEdit *inputField_ = nullptr;
  QLabel *chip_ = nullptr;
  QLabel *headline_ = nullptr;
  QLabel *preview_ = nullptr;
  QLabel *verification_ = nullptr;
  QLabel *details_ = nullptr;
  QPushButton *copy_ = nullptr;
  QPushButton *save_ = nullptr;
  QPushButton *clear_ = nullptr;
  QWidget *composer_ = nullptr;
  QVBoxLayout *composerLayout_ = nullptr;
};