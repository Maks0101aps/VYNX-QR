#pragma once

#include <QMainWindow>
#include <QString>
#include <QTimer>

#include <memory>

#include "vynx-qr-bridge/src/lib.rs.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class AboutDialog;
class ClipboardSuggestion;
class CustomizePanel;
class QrPreview;
class SettingsDialog;
class SpecialQrForms;

/// The window: state, wiring, and nothing else.
///
/// Every visual piece is a widget that owns itself. This class decides which
/// payload is current, asks the engine to render it, and hands the result to the
/// preview. It does not draw, and it does not encode anything.
class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  /// The size the preview renders at.
  ///
  /// Deliberately not the export size: a 2048 pixel render on every keystroke
  /// costs time and memory for an image the widget shows at a few hundred pixels.
  static constexpr std::uint32_t PreviewSize = 768;

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onSmartInputChanged();
  void onSmartInputSubmitted();
  void onClearInput();
  /// Switch between the normalised payload and exactly what was typed.
  void onToggleOriginal();
  void onCopy();
  void onSave();
  void onOpenSettings();
  void onOpenAbout();
  void onSaveAs();
  void onSpecialRequested();
  void onBackToSmart();
  void onCustomizeToggled();
  void onLogoAdded();
  void onLogoDropped(const QString &path);

private:
  /// Build the composer column: smart input, or a structured form.
  void buildComposer(QWidget *composer);
  void buildPreview(QWidget *pane);
  void wireShortcuts();

  /// Re-render for the current state, if there is anything to render.
  void refresh();

  /// What to encode right now, from whichever composer page is showing.
  [[nodiscard]] vynx::Payload currentPayload() const;

  /// Render options, with the panel's style and the loaded preferences merged.
  [[nodiscard]] vynx::RenderOptions currentOptions(std::uint32_t sizePx) const;

  /// Swap between the smart input and the structured forms.
  void setComposerMode(bool smart);

  /// Rebuild the detection row after the input changed.
  void updateChip();

  /// The offer to keep what was typed, shown only when something changed.
  void showOriginalToggle(bool visible, bool showingOriginal);

  void copyToClipboard();

  /// Save using the preferred format.
  void saveToDisk();

  /// Save as a specific format, chosen from the Save menu.
  void saveToDisk(vynx::ExportFormat format);
  void showError(const QString &message);
  void showToast(const QString &message);

  /// Read the clipboard once at start-up and either use it or offer it.
  void readClipboardOnce();

  /// Apply a settings change to the window, the theme and the engine.
  void applySettings(const vynx::Settings &settings);

  /// Ask the engine to load a logo and attach it, reporting any refusal.
  void attachLogo(const QString &path);

  /// Debounces detection so a fast typist does not trigger a render per key.
  QTimer debounce_;

  QString inputText_;
  QString detectedKind_;
  QString normalizationNotice_;
  bool detected_ = false;
  bool changesInput_ = false;
  bool useOriginalText_ = false;
  bool smartMode_ = true;
  bool customizeOpen_ = false;

  /// The payload the engine produced for the input, kept rather than rebuilt.
  ///
  /// Reconstructing it from `Analysis::encoded` is how `mailto:` ended up
  /// doubled: that string is the finished symbol content, not a domain value.
  vynx::Payload detectedPayload_;

  /// Bumped on every request; a result from an older generation is discarded, so
  /// typing quickly can never leave a stale code on screen.
  std::uint64_t generation_ = 0;
  vynx::GenerateResult current_;

  vynx::Settings settings_;

  QStackedWidget *composerStack_ = nullptr;
  QWidget *smartPage_ = nullptr;
  QLineEdit *inputField_ = nullptr;
  QWidget *chipRow_ = nullptr;
  QLabel *chip_ = nullptr;
  QPushButton *originalToggle_ = nullptr;
  SpecialQrForms *forms_ = nullptr;
  QrPreview *preview_ = nullptr;
  CustomizePanel *customize_ = nullptr;
  ClipboardSuggestion *clipboard_ = nullptr;
  QLabel *toast_ = nullptr;
  QPushButton *copy_ = nullptr;
  QPushButton *save_ = nullptr;
  QPushButton *clear_ = nullptr;
  QPushButton *customizeButton_ = nullptr;
  QPushButton *specialButton_ = nullptr;
  QPushButton *settingsButton_ = nullptr;
  QPushButton *aboutButton_ = nullptr;
  QVBoxLayout *composerLayout_ = nullptr;
};