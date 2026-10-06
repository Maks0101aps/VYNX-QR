#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFile>
#include <QSaveFile>
#include <QFileDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Theme.h"
#include "platform/PlatformIntegration.h"
#include "widgets/AboutDialog.h"
#include "widgets/Bridge.h"
#include "widgets/ClipboardSuggestion.h"
#include "widgets/CustomizePanel.h"
#include "widgets/SettingsDialog.h"
#include "widgets/QrPreview.h"
#include "widgets/SpecialQrForms.h"

namespace {

/// Payload kinds and what a scanner will do with them.
QString contentKindLabel(vynx::ContentKind kind) {
  if (kind == vynx::ContentKind::Url) return QStringLiteral("URL");
  if (kind == vynx::ContentKind::Email) return QStringLiteral("Email");
  if (kind == vynx::ContentKind::Phone) return QStringLiteral("Phone");
  return QStringLiteral("Text");
}

/// A file name a Windows shell accepts, built from the payload rather than from
/// the preview's pixel dimensions.
QString suggestName(const vynx::Payload &payload) {
  QString base = vynxq::toQString(vynx::payload_label(payload.kind));
  const QString raw = vynxq::toQString(payload.url.empty() ? payload.text : payload.url);
  if (!raw.isEmpty() && payload.kind != vynx::PayloadType::Text) {
    base = raw;
  } else if (payload.kind == vynx::PayloadType::Wifi) {
    base = QStringLiteral("wifi-%1").arg(vynxq::toQString(payload.wifi.ssid));
  } else if (payload.kind == vynx::PayloadType::VCard) {
    base = QStringLiteral("contact-%1 %2")
               .arg(vynxq::toQString(payload.vcard.first_name),
                    vynxq::toQString(payload.vcard.last_name));
  }

  base = base.toLower();
  for (QChar &character : base) {
    if (!character.isLetterOrNumber()) {
      character = QLatin1Char('-');
    }
  }
  while (base.contains(QStringLiteral("--"))) {
    base.replace(QStringLiteral("--"), QStringLiteral("-"));
  }
  base.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
  if (base.isEmpty()) {
    base = QStringLiteral("vynx-qr");
  }
  return base;
}

/// Read a file into the byte vector the bridge expects for a logo.
///
/// The engine does the decoding and the validation; this only moves bytes. An
/// unreadable file comes back empty, and the caller reports that itself rather
/// than inventing a bridge error, whose constructor is private by design.
rust::Vec<std::uint8_t> readFile(const QString &path) {
  rust::Vec<std::uint8_t> out;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return out;
  }
  const QByteArray bytes = file.readAll();
  file.close();
  for (const char byte : bytes) {
    out.push_back(static_cast<std::uint8_t>(byte));
  }
  return out;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("VYNX QR"));
  setMinimumSize(Tokens::WindowMinimumWidth, Tokens::WindowMinimumHeight);
  resize(Tokens::WindowWidth, Tokens::WindowHeight);

  settings_ = vynx::load_settings();

  auto *central = new QWidget(this);
  auto *outer = new QHBoxLayout(central);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  auto *composer = new QWidget(central);
  buildComposer(composer);
  outer->addWidget(composer);

  auto *pane = new QWidget(central);
  buildPreview(pane);
  outer->addWidget(pane, 1);

  setCentralWidget(central);

  debounce_.setSingleShot(true);
  debounce_.setInterval(150);
  // The whole point of the universal input is that the code appears as you type.
  // Without this connection the timer is configured and never runs, and the code
  // would only refresh on Enter.
  connect(&debounce_, &QTimer::timeout, this, &MainWindow::onSmartInputChanged);

  wireShortcuts();

  // Read once, after the window exists, so the first paint is not delayed.
  QTimer::singleShot(0, this, &MainWindow::readClipboardOnce);

  applyTheme();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildComposer(QWidget *composer) {
  composerLayout_ = new QVBoxLayout(composer);
  composerLayout_->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceLarge,
                                      Tokens::SpaceLarge, Tokens::SpaceLarge);
  composerLayout_->setSpacing(Tokens::SpaceMedium);

  auto *headline = new QLabel(QStringLiteral("Create a QR code"));
  headline->setProperty("role", QStringLiteral("heading"));
  composerLayout_->addWidget(headline);

  composerStack_ = new QStackedWidget(composer);

  // ------------------------------------------------------------ smart input --
  smartPage_ = new QWidget;
  auto *smartLayout = new QVBoxLayout(smartPage_);
  smartLayout->setContentsMargins(0, 0, 0, 0);
  smartLayout->setSpacing(Tokens::SpaceSmall);

  auto *hint = new QLabel(QStringLiteral("Paste a link, text, email address or phone number."));
  hint->setProperty("role", QStringLiteral("caption"));
  hint->setWordWrap(true);
  smartLayout->addWidget(hint);

  inputField_ = new QLineEdit;
  inputField_->setPlaceholderText(QStringLiteral("github.com or Ctrl+V"));
  inputField_->setAccessibleName(QStringLiteral("Create QR"));
  inputField_->setAccessibleDescription(
      QStringLiteral("Anything typed here is classified automatically and encoded."));
  inputField_->setClearButtonEnabled(true);
  smartLayout->addWidget(inputField_);

  chipRow_ = new QWidget(smartPage_);
  auto *chipLayout = new QHBoxLayout(chipRow_);
  chipLayout->setContentsMargins(0, 0, 0, 0);
  chipLayout->setSpacing(Tokens::SpaceSmall);
  chip_ = new QLabel(chipRow_);
  chip_->setProperty("role", QStringLiteral("caption"));
  chipLayout->addWidget(chip_);
  chipLayout->addStretch(1);
  originalToggle_ = new QPushButton(chipRow_);
  originalToggle_->setProperty("flat", true);
  originalToggle_->setCursor(Qt::PointingHandCursor);
  originalToggle_->setAccessibleName(QStringLiteral("Keep exactly what was typed"));
  originalToggle_->hide();
  chipLayout->addWidget(originalToggle_);
  chipRow_->hide();
  smartLayout->addWidget(chipRow_);

  clipboard_ = new ClipboardSuggestion(smartPage_);
  connect(clipboard_, &ClipboardSuggestion::accepted, this, [this](const QString &text) {
    inputText_ = text;
    inputField_->setText(text);
    onSmartInputChanged();
  });
  smartLayout->addWidget(clipboard_);

  smartLayout->addStretch(1);

  // --------------------------------------------------------- structured forms --
  forms_ = new SpecialQrForms(composer);
  connect(forms_, &SpecialQrForms::edited, this, [this] {
    ++generation_;
    refresh();
  });
  connect(forms_, &SpecialQrForms::backRequested, this, &MainWindow::onBackToSmart);

  composerStack_->addWidget(smartPage_);
  composerStack_->addWidget(forms_);
  composerLayout_->addWidget(composerStack_, 1);

  // -------------------------------------------------------------- customize --
  customize_ = new CustomizePanel(composer);
  customize_->applySettings(settings_);
  customize_->hide();
  connect(customize_, &CustomizePanel::edited, this, [this] {
    ++generation_;
    refresh();
  });
  connect(customize_, &CustomizePanel::addLogoRequested, this, &MainWindow::onLogoAdded);
  connect(customize_, &CustomizePanel::removeLogoRequested, this,
          [this] { customize_->removeLogo(); });
  composerLayout_->addWidget(customize_, 1);

  // ---------------------------------------------------------------- actions --
  auto *actions = new QHBoxLayout;
  actions->setSpacing(Tokens::SpaceSmall);

  copy_ = new QPushButton(QStringLiteral("Copy"), composer);
  copy_->setProperty("accent", true);
  copy_->setAccessibleName(QStringLiteral("Copy QR code image to the clipboard"));
  copy_->setEnabled(false);
  actions->addWidget(copy_);

  save_ = new QPushButton(QStringLiteral("Save ▾"), composer);
  save_->setAccessibleName(QStringLiteral("Save QR code"));
  save_->setEnabled(false);
  actions->addWidget(save_);

  clear_ = new QPushButton(QStringLiteral("Clear"), composer);
  clear_->setProperty("flat", true);
  clear_->setAccessibleName(QStringLiteral("Clear input"));
  actions->addWidget(clear_);

  customizeButton_ = new QPushButton(QStringLiteral("Customize"), composer);
  customizeButton_->setCheckable(true);
  customizeButton_->setCursor(Qt::PointingHandCursor);
  customizeButton_->setAccessibleName(QStringLiteral("Customize the QR code"));
  actions->addWidget(customizeButton_);

  composerLayout_->addLayout(actions);

  specialButton_ = new QPushButton(QStringLiteral("+  Special QR"), composer);
  specialButton_->setProperty("flat", true);
  specialButton_->setCursor(Qt::PointingHandCursor);
  specialButton_->setAccessibleName(QStringLiteral("Create a Wi-Fi, contact, email, phone, "
                                                    "message or location QR code"));
  composerLayout_->addWidget(specialButton_);

  toast_ = new QLabel(composer);
  toast_->setProperty("role", QStringLiteral("caption"));
  toast_->hide();
  composerLayout_->addWidget(toast_);

  connect(inputField_, &QLineEdit::textChanged, this, [this](const QString &text) {
    inputText_ = text;
    // Every keystroke invalidates whatever is on screen, so a fast typist can
    // never be left looking at a code for something they already deleted.
    ++generation_;
    // Debounced, so a render happens once the user pauses rather than per key.
    debounce_.start();
  });
  connect(inputField_, &QLineEdit::returnPressed, this, &MainWindow::onSmartInputSubmitted);
  connect(originalToggle_, &QPushButton::clicked, this, &MainWindow::onToggleOriginal);
  connect(copy_, &QPushButton::clicked, this, &MainWindow::onCopy);
  connect(save_, &QPushButton::clicked, this, &MainWindow::onSaveAs);
  connect(clear_, &QPushButton::clicked, this, &MainWindow::onClearInput);
  connect(customizeButton_, &QPushButton::clicked, this, &MainWindow::onCustomizeToggled);
  connect(specialButton_, &QPushButton::clicked, this, &MainWindow::onSpecialRequested);
}

void MainWindow::buildPreview(QWidget *pane) {
  auto *layout = new QVBoxLayout(pane);
  layout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceLarge, Tokens::SpaceLarge,
                             Tokens::SpaceLarge);
  layout->setSpacing(Tokens::SpaceSmall);

  auto *header = new QHBoxLayout;
  settingsButton_ = new QPushButton(QStringLiteral("Settings"), pane);
  settingsButton_->setProperty("flat", true);
  settingsButton_->setCursor(Qt::PointingHandCursor);
  settingsButton_->setAccessibleName(QStringLiteral("Settings"));
  header->addStretch(1);
  aboutButton_ = new QPushButton(QStringLiteral("About"), pane);
  aboutButton_->setProperty("flat", true);
  aboutButton_->setCursor(Qt::PointingHandCursor);
  aboutButton_->setAccessibleName(QStringLiteral("About VYNX QR"));
  header->addWidget(aboutButton_);
  header->addWidget(settingsButton_);
  layout->addLayout(header);

  preview_ = new QrPreview(pane);
  connect(preview_, &QrPreview::logoDropped, this, &MainWindow::onLogoDropped);
  layout->addWidget(preview_, 1);

  connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
  connect(aboutButton_, &QPushButton::clicked, this, &MainWindow::onOpenAbout);
}

void MainWindow::wireShortcuts() {
  // Copy stays a window shortcut only when the focus is not in a text field, so
  // Ctrl+C inside the input keeps copying text rather than stealing it.
  auto *copyShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+C")), this);
  copyShortcut->setContext(Qt::WindowShortcut);
  connect(copyShortcut, &QShortcut::activated, this, [this] {
    if (QApplication::focusWidget() == inputField_) {
      inputField_->copy();
      return;
    }
    onCopy();
  });

  auto *saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
  connect(saveShortcut, &QShortcut::activated, this, &MainWindow::onSave);

  auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
  connect(settingsShortcut, &QShortcut::activated, this, &MainWindow::onOpenSettings);

  auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(escape, &QShortcut::activated, this, [this] {
    if (customizeOpen_) {
      onCustomizeToggled();
      return;
    }
    if (!smartMode_) {
      onBackToSmart();
    }
  });
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // No tray icon, no watcher, no helper: closing the window ends the process.
  event->accept();
}

void MainWindow::setComposerMode(bool smart) {
  smartMode_ = smart;
  composerStack_->setCurrentIndex(smart ? 0 : 1);
  specialButton_->setText(smart ? QStringLiteral("+  Special QR")
                                : QStringLiteral("←  Smart input"));
  ++generation_;
  refresh();
}

void MainWindow::onSmartInputSubmitted() {
  debounce_.stop();
  onSmartInputChanged();
}

void MainWindow::onSmartInputChanged() {
  const QString trimmed = inputText_.trimmed();
  if (trimmed.isEmpty()) {
    showError(QString());
    return;
  }

  const std::string text = inputText_.toStdString();
  try {
    // Classification and the payload travel together on purpose. The payload is
    // the engine's, not something rebuilt from the encoded string, which already
    // carries a scheme and would be encoded a second time.
    const vynx::SmartPayload smart = vynx::smart_payload(rust::Str(text));
    detected_ = smart.detected;
    changesInput_ = smart.payload.kind != vynx::PayloadType::Text &&
        (smart.analysis.changes_input || inputText_ != trimmed);
    detectedPayload_ = smart.payload;
    detectedKind_ = contentKindLabel(smart.analysis.kind);
    normalizationNotice_ = vynxq::toQString(smart.analysis.normalization);
    // A new input invalidates the previous choice: the user is no longer looking
    // at the text they opted out of.
    useOriginalText_ = false;
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
    return;
  }

  updateChip();
  refresh();
}

void MainWindow::onToggleOriginal() {
  if (!changesInput_) {
    return;
  }
  useOriginalText_ = !useOriginalText_;
  ++generation_;
  updateChip();
  refresh();
}

void MainWindow::clearInput() { onClearInput(); }

void MainWindow::setSpecialKind(vynx::PayloadType kind) {
  setComposerMode(false);
  forms_->showKind(kind);
}

void MainWindow::onClearInput() {
  inputField_->clear();
  inputText_.clear();
  ++generation_;
  showError(QString());
}

void MainWindow::updateChip() {
  if (!detected_) {
    chipRow_->hide();
    return;
  }

  QStringList parts;
  parts << detectedKind_;
  if (useOriginalText_) {
    parts << QStringLiteral("using exactly what you typed");
  } else if (!normalizationNotice_.isEmpty()) {
    parts << normalizationNotice_;
  }
  chip_->setText(parts.join(QStringLiteral(" · ")));
  // The escape hatch exists only when the engine changed the text. Offering it
  // for an untouched URL would imply something was rewritten.
  showOriginalToggle(changesInput_, useOriginalText_);
}

void MainWindow::showOriginalToggle(bool visible, bool showingOriginal) {
  originalToggle_->setVisible(visible);
  // The label is what tells the user what the button does, so it is set here
  // rather than once at construction where a later state could leave it blank.
  originalToggle_->setText(showingOriginal ? QStringLiteral("Use detected %1").arg(detectedKind_)
                                          : QStringLiteral("Use original text"));
  chipRow_->setVisible(true);
}

vynx::Payload MainWindow::currentPayload() const {
  if (!smartMode_) {
    return forms_->payload();
  }
  // The escape hatch is a text payload built by the engine, so it goes through
  // the same typed path rather than being assembled here.
  if (useOriginalText_ || detectedPayload_.kind == vynx::PayloadType::Text) {
    return vynx::original_text_payload(rust::Str(inputText_.toStdString()));
  }
  // Otherwise the payload is the one the engine produced. Rebuilding it from
  // `Analysis::encoded` is what produced "mailto:mailto:hello@example.com".
  return detectedPayload_;
}

vynx::RenderOptions MainWindow::currentOptions(std::uint32_t sizePx) const {
  vynx::RenderOptions options = customize_->options(sizePx);
  // The export size is a preference; the preview keeps its own so a 2048
  // preference does not mean a 2048 render on every keystroke.
  return options;
}

void MainWindow::refresh() {
  const bool structuredAndIncomplete = !smartMode_ && !forms_->isComplete();
  const bool smartAndEmpty = smartMode_ && inputText_.trimmed().isEmpty();

  if (structuredAndIncomplete || smartAndEmpty) {
    // Going back to the empty state must drop the previous result, not merely
    // grey out the buttons: the old bytes would otherwise stay in memory and stay
    // reachable through Copy and Save.
    current_ = vynx::GenerateResult();
    preview_->showEmpty();
    copy_->setEnabled(false);
    save_->setEnabled(false);
    return;
  }

  const std::uint64_t generation = generation_;
  try {
    const vynx::GenerateResult result =
        vynx::generate(currentPayload(), currentOptions(PreviewSize));
    // A newer request has already been issued; this result is stale.
    if (generation != generation_) {
      return;
    }
    current_ = result;
    preview_->showResult(current_);
    copy_->setEnabled(true);
    save_->setEnabled(true);
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
  }
}

void MainWindow::showError(const QString &message) {
  if (message.isEmpty()) {
    // The empty state must not keep the previous result: Copy and Save are
    // disabled, but the bytes would still be sitting in memory and reachable.
    current_ = vynx::GenerateResult();
    preview_->showEmpty();
    copy_->setEnabled(false);
    save_->setEnabled(false);
    return;
  }
  preview_->showError(message);
  copy_->setEnabled(false);
  save_->setEnabled(false);
}

void MainWindow::showToast(const QString &message) {
  toast_->setText(message);
  toast_->show();
  QTimer::singleShot(2200, toast_, &QWidget::hide);
}

void MainWindow::onCopy() { copyToClipboard(); }

void MainWindow::copyToClipboard() {
  if (current_.width == 0) {
    return;
  }
  try {
    // Straight RGBA out of the renderer, so the clipboard holds exactly the
    // pixels that were verified.
    const vynx::Bitmap bitmap =
        vynx::generate_bitmap(currentPayload(), currentOptions(customize_->exportSize()));
    const QImage image(reinterpret_cast<const uchar *>(bitmap.pixels.data()),
                       static_cast<int>(bitmap.width), static_cast<int>(bitmap.height),
                       static_cast<qsizetype>(bitmap.width * 4), QImage::Format_RGBA8888);
    // Copied because `bitmap` dies at the end of this scope.
    QApplication::clipboard()->setImage(image.copy());
    copy_->setText(QStringLiteral("✓ Copied"));
    QTimer::singleShot(1400, this, [this] { copy_->setText(QStringLiteral("Copy")); });
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
  }
}

void MainWindow::onSave() { saveToDisk(); }

void MainWindow::onSaveAs() {
  if (current_.width == 0) {
    return;
  }
  // The menu offers both formats; the preference decides what Ctrl+S does, so a
  // change here is a one-off rather than a settings change.
  QMenu menu(this);
  menu.addAction(QStringLiteral("PNG image"), this, [this] {
    saveToDisk(vynx::ExportFormat::Png);
  });
  menu.addAction(QStringLiteral("SVG vector"), this, [this] {
    saveToDisk(vynx::ExportFormat::Svg);
  });
  menu.exec(save_->mapToGlobal(QPoint(0, 0)));
}

void MainWindow::saveToDisk() {
  saveToDisk(settings_.default_format);
}

void MainWindow::saveToDisk(vynx::ExportFormat format) {
  if (current_.width == 0) {
    return;
  }

  const vynx::Payload payload = currentPayload();
  const bool svg = format == vynx::ExportFormat::Svg;
  const QString suffix = svg ? QStringLiteral("svg") : QStringLiteral("png");
  const QString suggested = suggestName(payload) + QStringLiteral("-qr.") + suffix;

  const QString chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save QR code"), suggested,
      svg ? QStringLiteral("SVG vector (*.svg)") : QStringLiteral("PNG image (*.png)"));
  if (chosen.isEmpty()) {
    return;
  }

  exportToFile(chosen, format);
}

bool MainWindow::exportToFile(const QString &chosen, vynx::ExportFormat format) {
  const auto payload = currentPayload();
  const bool svg = format == vynx::ExportFormat::Svg;
  const auto size = customize_->exportSize();

  try {
    QByteArray bytes;
    if (svg || chosen.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
      // Written as text by the engine, so the file stays a real vector.
      bytes = vynxq::toQString(vynx::generate_svg(payload, currentOptions(size), size))
                  .toUtf8();
    } else {
      const vynx::GenerateResult result =
          vynx::generate(payload, currentOptions(size));
      bytes = QByteArray(reinterpret_cast<const char *>(result.png.data()),
                         static_cast<qsizetype>(result.png.size()));
    }

    QSaveFile file(chosen);
    if (!file.open(QIODevice::WriteOnly)) {
      showError(QStringLiteral("That location cannot be written to."));
      return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
      showError(QStringLiteral("The file could not be saved: %1").arg(file.errorString()));
      return false;
    }
    showToast(QStringLiteral("Saved to %1").arg(QFileInfo(chosen).fileName()));
    return true;
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
    return false;
  }
}

void MainWindow::onOpenSettings() {
  SettingsDialog dialog(this);
  connect(&dialog, &SettingsDialog::settingsChanged, this, &MainWindow::applySettings);
  dialog.exec();

  // Whatever the dialog left behind is persisted by the same call, so the file
  // and the window never disagree.
  applySettings(dialog.settings());
}

void MainWindow::applySettings(const vynx::Settings &settings) {
  settings_ = settings;
  customize_->applySettings(settings);
  applyTheme();
  try {
    vynx::save_settings(settings);
  } catch (const rust::Error &error) {
    showToast(vynxq::errorMessage(error));
  }
}

void MainWindow::applyTheme() {
  const QColor accent = settings_.use_windows_accent
      ? PlatformIntegration::systemAccentColor() : QColor();
  const auto palette = makePalette(
      settings_.theme == vynx::ThemeMode::Dark ? QStringLiteral("dark")
      : settings_.theme == vynx::ThemeMode::Light ? QStringLiteral("light")
                                                : QStringLiteral("system"), accent);
  QPalette widgetPalette;
  widgetPalette.setColor(QPalette::Window, palette.window);
  widgetPalette.setColor(QPalette::WindowText, palette.textPrimary);
  widgetPalette.setColor(QPalette::Base, palette.surfaceSunken);
  widgetPalette.setColor(QPalette::Text, palette.textPrimary);
  widgetPalette.setColor(QPalette::Button, palette.surface);
  widgetPalette.setColor(QPalette::ButtonText, palette.textPrimary);
  widgetPalette.setColor(QPalette::Highlight, palette.accent);
  widgetPalette.setColor(QPalette::HighlightedText, palette.accentText);
  qApp->setPalette(widgetPalette);
  qApp->setStyleSheet(styleSheetFor(palette));
  PlatformIntegration::applyWindowChrome(reinterpret_cast<void *>(winId()), palette.isDark());
}

void MainWindow::onOpenAbout() {
  AboutDialog dialog(this);
  dialog.exec();
}

void MainWindow::readClipboardOnce() {
  // Read once, at start-up, and never watched afterwards. Anything more would be
  // a background process watching the user's clipboard, which is not something
  // this application does.
  if (!settings_.clipboard_check) {
    return;
  }
  const QString text = QApplication::clipboard()->text();
  if (text.trimmed().isEmpty()) {
    return;
  }
  if (settings_.auto_paste) {
    inputText_ = text;
    inputField_->setText(inputText_);
    onSmartInputChanged();
    showToast(QStringLiteral("Encoded the clipboard contents"));
    return;
  }
  clipboard_->offer(text);
}

void MainWindow::onSpecialRequested() {
  if (!smartMode_) {
    setComposerMode(true);
    return;
  }

  QMenu menu(this);
  const QList<QPair<QString, vynx::PayloadType>> kinds = {
      {QStringLiteral("Wi-Fi"), vynx::PayloadType::Wifi},
      {QStringLiteral("Contact"), vynx::PayloadType::VCard},
      {QStringLiteral("Email"), vynx::PayloadType::Email},
      {QStringLiteral("Phone"), vynx::PayloadType::Phone},
      {QStringLiteral("Message"), vynx::PayloadType::Sms},
      {QStringLiteral("Location"), vynx::PayloadType::Geo},
  };
  for (const auto &entry : kinds) {
    menu.addAction(entry.first, this, [this, entry] {
      setComposerMode(false);
      forms_->showKind(entry.second);
    });
  }
  menu.exec(specialButton_->mapToGlobal(QPoint(0, 0)));
}

void MainWindow::onBackToSmart() { setComposerMode(true); }

void MainWindow::onCustomizeToggled() {
  customizeOpen_ = !customizeOpen_;
  customizeButton_->setChecked(customizeOpen_);
  customize_->setVisible(customizeOpen_);
}

void MainWindow::onLogoAdded() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Choose a logo"), QString(),
      QStringLiteral("Images (*.png *.jpg *.jpeg *.webp)"));
  if (!path.isEmpty()) {
    attachLogo(path);
  }
}

void MainWindow::onLogoDropped(const QString &path) { attachLogo(path); }

void MainWindow::attachLogo(const QString &path) {
  try {
    // The engine validates the file and is the only decoder; the window just
    // hands it over and reports whatever it says.
    const vynx::LogoAsset asset = vynx::load_logo(vynxq::toRust(path));
    vynx::Logo logo;
    logo.name = vynxq::toRust(QString::fromStdString(std::string(asset.name.data(), asset.name.size())));
    logo.data = readFile(path);
    if (logo.data.empty()) {
      showError(QStringLiteral("That image could not be read."));
      return;
    }
    customize_->setLogo(logo);
    // The panel is hidden when the logo is added from a drop, so open it.
    if (!customizeOpen_) {
      onCustomizeToggled();
    }
    showToast(QStringLiteral("Logo added"));
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
  }
}
