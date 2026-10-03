#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QStyle>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#include "Theme.h"
#include "WindowsIntegration.h"

namespace {

/// The engine hands back a Rust owned byte vector; Qt wants a QByteArray.
QByteArray toByteArray(const rust::Vec<std::uint8_t> &bytes) {
  return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                    static_cast<qsizetype>(bytes.size()));
}

/// A `rust::String` becomes a QString without a lossy conversion.
QString toQString(const rust::String &value) {
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

/// A `rust::String` becomes a std::string, which is what QFileDialog wants.
std::string toStdString(const rust::String &value) {
  return std::string(value.data(), value.size());
}

QString verifyLabel(vynx::VerifyStatus status) {
  if (status == vynx::VerifyStatus::Failed) {
    return QStringLiteral("QR could not be verified");
  }
  if (status == vynx::VerifyStatus::Mismatch) {
    return QStringLiteral("Verification mismatch");
  }
  return QStringLiteral("Scan verified");
}

QString ecLabel(vynx::EcLevel level) {
  if (level == vynx::EcLevel::H) return QStringLiteral("High");
  if (level == vynx::EcLevel::Q) return QStringLiteral("Quartile");
  if (level == vynx::EcLevel::M) return QStringLiteral("Medium");
  return QStringLiteral("Low");
}

/// The name the detector gave this input, for the status chip.
QString contentKindLabel(vynx::ContentKind kind) {
  if (kind == vynx::ContentKind::Url) return QStringLiteral("URL");
  if (kind == vynx::ContentKind::Email) return QStringLiteral("Email");
  if (kind == vynx::ContentKind::Phone) return QStringLiteral("Phone");
  return QStringLiteral("Text");
}

/// The engine's own explanation of a failure, always safe to show.
QString engineError(const rust::Error &error) {
  const QString message = QString::fromUtf8(error.what());
  return message.isEmpty() ? QStringLiteral("The QR engine reported a problem.") : message;
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

  // ------------------------------------------------------------- composer ---
  composer_ = new QWidget(central);
  composerLayout_ = new QVBoxLayout(composer_);
  composerLayout_->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                                      Tokens::SpaceXLarge, Tokens::SpaceLarge);
  composerLayout_->setSpacing(Tokens::SpaceMedium);

  auto *headline = new QLabel(QStringLiteral("Create a QR code"), composer_);
  headline->setProperty("role", QStringLiteral("heading"));
  composerLayout_->addWidget(headline);

  auto *hint = new QLabel(QStringLiteral("Paste a link, text, email address or phone number."),
                          composer_);
  hint->setProperty("role", QStringLiteral("caption"));
  hint->setWordWrap(true);
  composerLayout_->addWidget(hint);

  inputField_ = new QLineEdit(composer_);
  inputField_->setPlaceholderText(QStringLiteral("github.com or Ctrl+V"));
  inputField_->setAccessibleName(QStringLiteral("Create QR"));
  inputField_->setAccessibleDescription(
      QStringLiteral("Anything typed here is classified automatically and encoded."));
  inputField_->setClearButtonEnabled(true);
  composerLayout_->addWidget(inputField_);

chipRow_ = new QWidget(composer_);
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
  chipLayout->addWidget(originalToggle_);

  chipRow_->hide();
  composerLayout_->addWidget(chipRow_);

  // The spacer keeps the action row at the bottom, so the preview stays the
  // visual centre of the window however long the input is.
  composerLayout_->addStretch(1);

  auto *actions = new QHBoxLayout;
  actions->setSpacing(Tokens::SpaceSmall);

  copy_ = new QPushButton(QStringLiteral("Copy"), composer_);
  copy_->setProperty("accent", true);
  copy_->setAccessibleName(QStringLiteral("Copy QR code image to the clipboard"));
  copy_->setEnabled(false);
  actions->addWidget(copy_);

  save_ = new QPushButton(QStringLiteral("Save"), composer_);
  save_->setAccessibleName(QStringLiteral("Save QR code"));
  save_->setEnabled(false);
  actions->addWidget(save_);

  clear_ = new QPushButton(QStringLiteral("Clear"), composer_);
  clear_->setProperty("flat", true);
  clear_->setAccessibleName(QStringLiteral("Clear input"));
  actions->addWidget(clear_);

  composerLayout_->addLayout(actions);
  outer->addWidget(composer_);

  // -------------------------------------------------------------- preview ---
  auto *previewPane = new QWidget(central);
  auto *previewLayout = new QVBoxLayout(previewPane);
  previewLayout->setContentsMargins(Tokens::SpaceXLarge, Tokens::SpaceXLarge,
                                    Tokens::SpaceXLarge, Tokens::SpaceLarge);
  previewLayout->setSpacing(Tokens::SpaceMedium);

  preview_ = new QLabel(previewPane);
  preview_->setAlignment(Qt::AlignCenter);
  preview_->setMinimumSize(Tokens::PreviewMaximum, Tokens::PreviewMaximum);
  preview_->setAccessibleName(QStringLiteral("Generated QR code"));
  previewLayout->addWidget(preview_, 1);

  verification_ = new QLabel(previewPane);
  verification_->setAlignment(Qt::AlignCenter);
  verification_->hide();
  previewLayout->addWidget(verification_);

  details_ = new QLabel(previewPane);
  details_->setAlignment(Qt::AlignCenter);
  details_->setProperty("role", QStringLiteral("caption"));
  details_->hide();
  previewLayout->addWidget(details_);

  outer->addWidget(previewPane, 1);
  setCentralWidget(central);

  showEmptyState();

  // --------------------------------------------------------------- wiring ---
  debounce_.setSingleShot(true);
  debounce_.setInterval(150);
  connect(&debounce_, &QTimer::timeout, this, &MainWindow::onInputChanged);
  connect(inputField_, &QLineEdit::textChanged, this, [this](const QString &text) {
    inputText_ = text;
    // Every keystroke invalidates whatever is on screen.
    ++generation_;
    debounce_.start();
  });
  connect(inputField_, &QLineEdit::returnPressed, this, &MainWindow::onInputSubmitted);
  connect(copy_, &QPushButton::clicked, this, &MainWindow::onCopy);
  connect(save_, &QPushButton::clicked, this, &MainWindow::onSave);
  connect(clear_, &QPushButton::clicked, this, &MainWindow::onClearInput);
  connect(originalToggle_, &QPushButton::clicked, this, &MainWindow::onToggleOriginal);

  auto *copyShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+C")), this);
  connect(copyShortcut, &QShortcut::activated, this, &MainWindow::onCopy);
  auto *saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
  connect(saveShortcut, &QShortcut::activated, this, &MainWindow::onSave);

  WindowsIntegration::applyWindowChrome(reinterpret_cast<void *>(winId()));
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event) {
  // No tray icon, no watcher, no helper: closing the window ends the process.
  event->accept();
}

void MainWindow::onInputChanged() {
  const QString trimmed = inputText_.trimmed();
  if (trimmed.isEmpty()) {
    showEmptyState();
    return;
  }

  const std::string text = trimmed.toStdString();
  try {
    // Classification and the payload travel together on purpose. The payload is
    // the engine's, not something rebuilt here from the encoded string, which
    // already carries a scheme and would be encoded a second time.
    const vynx::SmartPayload smart = vynx::smart_payload(rust::Str(text));

    detected_ = smart.detected;
    changesInput_ = smart.analysis.changes_input;
    detectedPayload_ = smart.payload;
    detectedKind_ = contentKindLabel(smart.analysis.kind);
    normalizationNotice_ = toQString(smart.analysis.normalization);

    // A new input invalidates the previous choice: the user is no longer looking
    // at the text they opted out of.
    useOriginalText_ = false;
  } catch (const rust::Error &error) {
    showError(engineError(error));
    return;
  }

  updateChip();
  refresh();
}

void MainWindow::onToggleOriginal() {
  // Only offered when the detector actually changed something, so toggling it
  // always has an effect the user can see.
  if (!changesInput_) {
    return;
  }
  useOriginalText_ = !useOriginalText_;
  ++generation_;
  updateChip();
  refresh();
}

void MainWindow::onInputSubmitted() {
  debounce_.stop();
  onInputChanged();
}

void MainWindow::onClearInput() {
  inputField_->clear();
  inputText_.clear();
  ++generation_;
  showEmptyState();
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
  originalToggle_->setVisible(changesInput_ && !useOriginalText_);
  originalToggle_->setText(QStringLiteral("Use original text"));
  chipRow_->show();
}

void MainWindow::refresh() {
  const std::uint64_t generation = generation_;
  try {
    const vynx::Payload payload = currentPayload();
    const vynx::RenderOptions options = currentOptions(PreviewSize);
    const vynx::GenerateResult result = vynx::generate(payload, options);

    // A newer request has already been issued; this result is stale and must not
    // overwrite the newer one.
    if (generation != generation_) {
      return;
    }

    current_ = result;
    const QImage image = QImage::fromData(toByteArray(current_.png), "PNG");
    if (image.isNull()) {
      showError(QStringLiteral("The engine produced an image this build cannot read."));
      return;
    }
    showPreview(image);
  } catch (const rust::Error &error) {
    showError(engineError(error));
  }
}

void MainWindow::showEmptyState() {
  preview_->setPixmap(QPixmap());
  preview_->setText(QStringLiteral("Paste or type anything"));
  details_->hide();
  verification_->hide();
  copy_->setEnabled(false);
  save_->setEnabled(false);
  chip_->hide();
  detected_ = false;
  useOriginalText_ = false;
  normalizationNotice_.clear();
  current_ = vynx::GenerateResult();
}

void MainWindow::showPreview(const QImage &image) {
  // Fast transformation keeps the module edges hard. Smooth scaling would blur
  // the very thing that makes a QR code readable.
  preview_->setPixmap(QPixmap::fromImage(image.scaled(
      Tokens::PreviewMaximum, Tokens::PreviewMaximum, Qt::KeepAspectRatio,
      Qt::FastTransformation)));
  preview_->setText(QString());

  setVerification(current_.verification_status);
  copy_->setEnabled(true);
  save_->setEnabled(true);

  details_->setText(QStringLiteral("Version %1 · %2 modules · Error correction %3 · Contrast %4:1")
                        .arg(current_.version)
                        .arg(current_.modules)
                        .arg(ecLabel(current_.ec_level))
                        .arg(current_.contrast, 0, 'f', 1));
  details_->show();
}

void MainWindow::setVerification(vynx::VerifyStatus status) {
  verification_->setText(verifyLabel(status));
  verification_->setProperty("role",
                              status == vynx::VerifyStatus::Verified
                                  ? QStringLiteral("verificationGood")
                                  : QStringLiteral("verificationBad"));
  // Re-polish so a property change takes effect on an already visible label.
  verification_->style()->unpolish(verification_);
  verification_->style()->polish(verification_);
  verification_->show();
}

void MainWindow::showError(const QString &message) {
  preview_->setPixmap(QPixmap());
  preview_->setText(QStringLiteral("Nothing to show"));
  verification_->setText(message);
  verification_->setProperty("role", QStringLiteral("verificationBad"));
  verification_->style()->unpolish(verification_);
  verification_->style()->polish(verification_);
  verification_->show();
  details_->hide();
  copy_->setEnabled(false);
  save_->setEnabled(false);
}

bool MainWindow::hasCode() const { return current_.width > 0; }

vynx::Payload MainWindow::currentPayload() const {
  // The escape hatch is a text payload built by the engine, so it goes through
  // the same typed path rather than being assembled here.
  if (useOriginalText_) {
    return vynx::original_text_payload(rust::Str(inputText_.trimmed().toStdString()));
  }
  // Otherwise the payload is the one the engine produced. Rebuilding it from
  // `Analysis::encoded` is what produced "mailto:mailto:hello@example.com":
  // that string is the finished symbol content, not a domain value.
  return detectedPayload_;
}

vynx::RenderOptions MainWindow::currentOptions(std::uint32_t size) const {
  vynx::RenderOptions options = vynx::default_style();
  options.size_px = size;
  options.ec_level = settings_.default_error_correction;
  options.module_style = settings_.default_module_style;
  options.has_logo = false;
  return options;
}

void MainWindow::copyToClipboard() {
  if (!hasCode()) {
    return;
  }
  try {
    // Straight RGBA straight out of the renderer, so the clipboard holds exactly
    // the pixels that were verified.
    const auto bitmap =
        vynx::generate_bitmap(currentPayload(), currentOptions(settings_.default_size));
    const QImage image(reinterpret_cast<const uchar *>(bitmap.pixels.data()),
                       static_cast<int>(bitmap.width), static_cast<int>(bitmap.height),
                       static_cast<qsizetype>(bitmap.width * 4), QImage::Format_RGBA8888);
    // Copied because `bitmap` dies at the end of this scope.
    QApplication::clipboard()->setImage(image.copy());

    copy_->setText(QStringLiteral("Copied"));
    QTimer::singleShot(1600, this, [this] { copy_->setText(QStringLiteral("Copy")); });
  } catch (const rust::Error &error) {
    showError(engineError(error));
  }
}

void MainWindow::onCopy() { copyToClipboard(); }

void MainWindow::saveToDisk() {
  if (!hasCode()) {
    return;
  }
  const QString suggested =
      QFileDialog::getSaveFileName(this, QStringLiteral("Save QR code"),
                                   QStringLiteral("vynx-qr.png"),
                                   QStringLiteral("PNG image (*.png)"));
  if (suggested.isEmpty()) {
    return;
  }

  try {
    // Rendered at the export size, not reused from the preview, so the file is
    // the quality the user asked for.
    const auto result =
        vynx::generate(currentPayload(), currentOptions(settings_.default_size));
    QFile file(suggested);
    if (!file.open(QIODevice::WriteOnly)) {
      showError(QStringLiteral("That location cannot be written to."));
      return;
    }
    file.write(toByteArray(result.png));
    file.close();
    details_->setText(QStringLiteral("Saved to %1").arg(QFileInfo(suggested).fileName()));
    details_->show();
  } catch (const rust::Error &error) {
    showError(engineError(error));
  }
}

void MainWindow::onSave() { saveToDisk(); }

void MainWindow::onOpenSettings() {
  // The settings panel arrives with the next milestone; the shortcut exists now so
  // the key is never silently dead.
}