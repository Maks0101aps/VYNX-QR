#include "MainWindow.h"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QComboBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QSaveFile>
#include <QFileDialog>
#include <QFrame>
#include <QRegularExpression>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "Theme.h"
#include "i18n/I18n.h"
#include "platform/PlatformIntegration.h"
#include "widgets/AboutDialog.h"
#include "widgets/Bridge.h"
#include "widgets/ClipboardSuggestion.h"
#include "widgets/CustomizePanel.h"
#include "widgets/ScanResultDialog.h"
#include "widgets/ScreenSnipper.h"
#include "widgets/SettingsDialog.h"
#include "widgets/QrPreview.h"
#include "widgets/SpecialQrForms.h"
#include "widgets/StyleSelectorWidget.h"
#include "updater/AutoUpdater.h"
#include "updater/UpdateDialog.h"

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
  } else if (payload.kind == vynx::PayloadType::Event) {
    base = QStringLiteral("event-%1").arg(vynxq::toQString(payload.event.title));
  } else if (payload.kind == vynx::PayloadType::Totp) {
    base = QStringLiteral("totp-%1").arg(vynxq::toQString(payload.totp.label));
  } else if (payload.kind == vynx::PayloadType::Crypto) {
    base = QStringLiteral("crypto-%1").arg(vynxq::toQString(payload.crypto.currency));
  } else if (payload.kind == vynx::PayloadType::Payment) {
    base = QStringLiteral("payment-%1").arg(vynxq::toQString(payload.payment.name));
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

QPixmap makeColorSwatchPixmap(const QColor &color, int size = 16) {
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter painter(&px);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(color);
  painter.setPen(QPen(QColor("#9CA3AF"), 1));
  painter.drawRoundedRect(0, 0, size - 1, size - 1, 3.0, 3.0);
  return px;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(I18n::tr("VYNX QR"));
  setMinimumSize(Tokens::WindowMinimumWidth, Tokens::WindowMinimumHeight);
  resize(Tokens::WindowWidth, Tokens::WindowHeight);

  settings_ = vynx::load_settings();
  if (settings_.language == vynx::AppLanguage::Ru) {
    I18n::instance().setLanguage(I18n::Language::Russian);
  } else if (settings_.language == vynx::AppLanguage::Uk) {
    I18n::instance().setLanguage(I18n::Language::Ukrainian);
  } else {
    I18n::instance().setLanguage(I18n::Language::English);
  }

  qrColor_ = QColor("#000000");
  bgColor_ = QColor("#FFFFFF");

  auto *central = new QWidget(this);
  auto *rootLayout = new QVBoxLayout(central);
  rootLayout->setContentsMargins(Tokens::SpaceLarge, Tokens::SpaceMedium,
                                 Tokens::SpaceLarge, Tokens::SpaceLarge);
  rootLayout->setSpacing(Tokens::SpaceMedium);

  // 1. Top bar
  auto *topBar = new QWidget(central);
  buildTopBar(topBar);
  rootLayout->addWidget(topBar);

  // 2. Two-column cards
  auto *columnsLayout = new QHBoxLayout;
  columnsLayout->setSpacing(Tokens::SpaceLarge);

  auto *composer = new QWidget(central);
  buildComposer(composer);
  columnsLayout->addWidget(composer, 11);

  auto *pane = new QWidget(central);
  buildPreview(pane);
  columnsLayout->addWidget(pane, 9);

  rootLayout->addLayout(columnsLayout, 1);
  setCentralWidget(central);

  debounce_.setSingleShot(true);
  debounce_.setInterval(150);
  connect(&debounce_, &QTimer::timeout, this, &MainWindow::onSmartInputChanged);

  wireShortcuts();
  updateColorSwatches();
  retranslateUi();

  QTimer::singleShot(0, this, &MainWindow::readClipboardOnce);

  applyTheme();
  PlatformIntegration::observeSystemTheme(this, [this] { applyTheme(); });

  snipper_ = new ScreenSnipper(this);
  connect(snipper_, &ScreenSnipper::codeDecoded, this, &MainWindow::onQrDecoded);
  connect(snipper_, &ScreenSnipper::decodeFailed, this, [this] {
    showToast(I18n::tr("QR code not found"));
  });

  setAcceptDrops(true);

  autoUpdater_ = new AutoUpdater(this);
  connect(autoUpdater_, &AutoUpdater::updateAvailable, this, &MainWindow::onUpdateAvailable);

  if (!qEnvironmentVariableIsSet("VYNX_NO_UPDATE_CHECK")) {
    QTimer::singleShot(2000, this, [this] {
      autoUpdater_->checkForUpdates(true);
    });
  }
}

MainWindow::~MainWindow() = default;

void MainWindow::buildTopBar(QWidget *topBar) {
  auto *layout = new QHBoxLayout(topBar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  auto *logoLabel = new QLabel(topBar);
  logoLabel->setPixmap(QIcon(QStringLiteral(":/icons/vynx-qr.png")).pixmap(24, 24));
  layout->addWidget(logoLabel);

  appTitleLabel_ = new QLabel(I18n::tr("QR Generator"), topBar);
  appTitleLabel_->setProperty("role", QStringLiteral("heading"));
  QFont f = appTitleLabel_->font();
  f.setBold(true);
  f.setPointSize(14);
  appTitleLabel_->setFont(f);
  layout->addWidget(appTitleLabel_);

  layout->addStretch(1);

  themeToggle_ = new QToolButton(topBar);
  themeToggle_->setObjectName(QStringLiteral("themeToggle"));
  themeToggle_->setProperty("role", QStringLiteral("topBarButton"));
  themeToggle_->setCursor(Qt::PointingHandCursor);
  themeToggle_->setText(QStringLiteral("🌙"));
  themeToggle_->setToolTip(I18n::tr("Toggle theme"));
  layout->addWidget(themeToggle_);

  scanButton_ = new QToolButton(topBar);
  scanButton_->setObjectName(QStringLiteral("scanButton"));
  scanButton_->setProperty("role", QStringLiteral("topBarButton"));
  scanButton_->setCursor(Qt::PointingHandCursor);
  scanButton_->setText(QStringLiteral("📷  ") + I18n::tr("Scan"));
  scanButton_->setToolTip(I18n::tr("Scan QR Code"));
  layout->addWidget(scanButton_);

  aboutButton_ = new QToolButton(topBar);
  aboutButton_->setObjectName(QStringLiteral("aboutButton"));
  aboutButton_->setProperty("role", QStringLiteral("topBarButton"));
  aboutButton_->setCursor(Qt::PointingHandCursor);
  aboutButton_->setText(I18n::tr("About"));
  aboutButton_->setToolTip(I18n::tr("About VYNX QR"));
  layout->addWidget(aboutButton_);

  settingsButton_ = new QToolButton(topBar);
  settingsButton_->setObjectName(QStringLiteral("settingsButton"));
  settingsButton_->setProperty("role", QStringLiteral("topBarButton"));
  settingsButton_->setCursor(Qt::PointingHandCursor);
  settingsButton_->setText(I18n::tr("Settings"));
  settingsButton_->setToolTip(I18n::tr("Settings"));
  layout->addWidget(settingsButton_);

  connect(themeToggle_, &QToolButton::clicked, this, &MainWindow::onThemeToggle);
  connect(scanButton_, &QToolButton::clicked, this, &MainWindow::onScanMenuRequested);
  connect(aboutButton_, &QToolButton::clicked, this, &MainWindow::onOpenAbout);
  connect(settingsButton_, &QToolButton::clicked, this, &MainWindow::onOpenSettings);
}

void MainWindow::buildComposer(QWidget *composer) {
  composerLayout_ = new QVBoxLayout(composer);
  composerLayout_->setContentsMargins(0, 0, 0, 0);

  auto *composerCard = new QFrame(composer);
  composerCard->setProperty("role", QStringLiteral("card"));
  auto *cardLayout = new QVBoxLayout(composerCard);
  cardLayout->setContentsMargins(Tokens::SpaceMedium, Tokens::SpaceMedium,
                                 Tokens::SpaceMedium, Tokens::SpaceMedium);
  cardLayout->setSpacing(Tokens::SpaceMedium);

  // 1. Header
  auto *headerLayout = new QVBoxLayout;
  headerLayout->setSpacing(4);
  composerHeading_ = new QLabel(I18n::tr("Create QR code"), composerCard);
  composerHeading_->setProperty("role", QStringLiteral("heading"));
  headerLayout->addWidget(composerHeading_);

  composerSubtitle_ = new QLabel(
      I18n::tr("Convert links, text, email, or phone numbers into QR codes"),
      composerCard);
  composerSubtitle_->setProperty("role", QStringLiteral("subheading"));
  composerSubtitle_->setWordWrap(true);
  headerLayout->addWidget(composerSubtitle_);
  cardLayout->addLayout(headerLayout);

  // 2. Category pills
  auto *pillsLayout = new QHBoxLayout;
  pillsLayout->setSpacing(Tokens::SpaceSmall);
  pillGroup_ = new QButtonGroup(this);
  pillGroup_->setExclusive(true);

  auto addPill = [this, pillsLayout](QPushButton *&pill, const QString &objName, const QString &text) {
    pill = new QPushButton(text, this);
    pill->setObjectName(objName);
    pill->setProperty("role", QStringLiteral("pill"));
    pill->setCheckable(true);
    pill->setCursor(Qt::PointingHandCursor);
    pillGroup_->addButton(pill);
    pillsLayout->addWidget(pill);
  };

  addPill(urlPill_, QStringLiteral("pill_url"), QStringLiteral("🔗 ") + I18n::tr("Link"));
  addPill(textPill_, QStringLiteral("pill_text"), QStringLiteral("T ") + I18n::tr("Text"));
  addPill(emailPill_, QStringLiteral("pill_email"), QStringLiteral("✉ ") + I18n::tr("Email"));
  addPill(phonePill_, QStringLiteral("pill_phone"), QStringLiteral("📞 ") + I18n::tr("Phone"));
  addPill(wifiPill_, QStringLiteral("pill_wifi"), QStringLiteral("📶 ") + I18n::tr("Wi-Fi"));
  addPill(morePill_, QStringLiteral("pill_more"), QStringLiteral("••• ") + I18n::tr("More") + QStringLiteral(" ▾"));
  pillsLayout->addStretch(1);
  cardLayout->addLayout(pillsLayout);

  urlPill_->setChecked(true);
  setupMoreMenu();

  // 3. Dynamic input container (stacked)
  composerStack_ = new QStackedWidget(composerCard);

  smartPage_ = new QWidget;
  auto *smartLayout = new QVBoxLayout(smartPage_);
  smartLayout->setContentsMargins(0, 0, 0, 0);
  smartLayout->setSpacing(Tokens::SpaceSmall);

  auto *inputRow = new QHBoxLayout;
  inputRow->setSpacing(Tokens::SpaceSmall);
  leadingIconLabel_ = new QLabel(QStringLiteral("🔗"), smartPage_);
  QFont iconFont = leadingIconLabel_->font();
  iconFont.setPointSize(12);
  leadingIconLabel_->setFont(iconFont);
  inputRow->addWidget(leadingIconLabel_);

  inputField_ = new QLineEdit(smartPage_);
  inputField_->setPlaceholderText(QStringLiteral("https://example.com"));
  inputField_->setAccessibleName(QStringLiteral("Create QR"));
  inputField_->setAccessibleDescription(
      QStringLiteral("Anything typed here is classified automatically and encoded."));
  inputField_->setClearButtonEnabled(true);
  inputRow->addWidget(inputField_, 1);
  smartLayout->addLayout(inputRow);

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

  forms_ = new SpecialQrForms(composerCard);
  connect(forms_, &SpecialQrForms::edited, this, [this] {
    ++generation_;
    refresh();
  });
  connect(forms_, &SpecialQrForms::backRequested, this, &MainWindow::onBackToSmart);

  composerStack_->addWidget(smartPage_);
  composerStack_->addWidget(forms_);
  cardLayout->addWidget(composerStack_);

  // 4. Settings Block
  styleHeading_ = new QLabel(I18n::tr("QR Code Style"), composerCard);
  styleHeading_->setProperty("role", QStringLiteral("subheading"));
  cardLayout->addWidget(styleHeading_);

  styleSelector_ = new StyleSelectorWidget(composerCard);
  styleSelector_->setStyle(settings_.default_module_style);
  connect(styleSelector_, &StyleSelectorWidget::styleChanged, this, &MainWindow::onStyleChanged);
  cardLayout->addWidget(styleSelector_);

  auto *colorLayout = new QHBoxLayout;
  colorLayout->setSpacing(Tokens::SpaceMedium);

  auto *fgBox = new QVBoxLayout;
  fgColorLabel_ = new QLabel(I18n::tr("QR Color"), composerCard);
  fgColorLabel_->setProperty("role", QStringLiteral("caption"));
  fgColorButton_ = new QPushButton(composerCard);
  fgColorButton_->setCursor(Qt::PointingHandCursor);
  fgBox->addWidget(fgColorLabel_);
  fgBox->addWidget(fgColorButton_);
  colorLayout->addLayout(fgBox, 1);

  auto *bgBox = new QVBoxLayout;
  bgColorLabel_ = new QLabel(I18n::tr("Background Color"), composerCard);
  bgColorLabel_->setProperty("role", QStringLiteral("caption"));
  bgColorButton_ = new QPushButton(composerCard);
  bgColorButton_->setCursor(Qt::PointingHandCursor);
  bgBox->addWidget(bgColorLabel_);
  bgBox->addWidget(bgColorButton_);
  colorLayout->addLayout(bgBox, 1);

  cardLayout->addLayout(colorLayout);

  connect(fgColorButton_, &QPushButton::clicked, this, [this] { onPickColor(true); });
  connect(bgColorButton_, &QPushButton::clicked, this, [this] { onPickColor(false); });

  // 5. Bottom buttons
  auto *bottomLayout = new QHBoxLayout;
  bottomLayout->setSpacing(Tokens::SpaceSmall);

  generateButton_ = new QPushButton(I18n::tr("Generate QR code"), composerCard);
  generateButton_->setProperty("accent", true);
  generateButton_->setCursor(Qt::PointingHandCursor);
  bottomLayout->addWidget(generateButton_, 2);

  clear_ = new QPushButton(I18n::tr("Clear"), composerCard);
  clear_->setProperty("flat", true);
  clear_->setAccessibleName(QStringLiteral("Clear input"));
  clear_->setCursor(Qt::PointingHandCursor);
  bottomLayout->addWidget(clear_, 1);

  cardLayout->addLayout(bottomLayout);

  toast_ = new QLabel(composerCard);
  toast_->setProperty("role", QStringLiteral("caption"));
  toast_->hide();
  cardLayout->addWidget(toast_);

  // Hidden compatibility widgets
  customize_ = new CustomizePanel(composerCard);
  customize_->applySettings(settings_);
  customize_->hide();
  connect(customize_, &CustomizePanel::edited, this, [this] {
    ++generation_;
    refresh();
  });
  connect(customize_, &CustomizePanel::addLogoRequested, this, &MainWindow::onLogoAdded);
  connect(customize_, &CustomizePanel::removeLogoRequested, this,
          [this] { customize_->removeLogo(); });

  customizeButton_ = new QPushButton(composerCard);
  customizeButton_->hide();

  specialButton_ = new QPushButton(composerCard);
  specialButton_->hide();

  composerLayout_->addWidget(composerCard);

  // Wire pill actions
  connect(urlPill_, &QPushButton::clicked, this, [this] {
    setComposerMode(true);
    leadingIconLabel_->setText(QStringLiteral("🔗"));
    inputField_->setPlaceholderText(QStringLiteral("https://example.com"));
    inputField_->setFocus();
  });

  connect(textPill_, &QPushButton::clicked, this, [this] {
    setComposerMode(true);
    leadingIconLabel_->setText(QStringLiteral("T"));
    inputField_->setPlaceholderText(I18n::tr("Enter text to encode..."));
    inputField_->setFocus();
  });

  connect(emailPill_, &QPushButton::clicked, this, [this] {
    setSpecialKind(vynx::PayloadType::Email);
  });

  connect(phonePill_, &QPushButton::clicked, this, [this] {
    setSpecialKind(vynx::PayloadType::Phone);
  });

  connect(wifiPill_, &QPushButton::clicked, this, [this] {
    setSpecialKind(vynx::PayloadType::Wifi);
  });

  connect(inputField_, &QLineEdit::textChanged, this, [this](const QString &text) {
    inputText_ = text;
    ++generation_;
    debounce_.start();
  });
  connect(inputField_, &QLineEdit::returnPressed, this, &MainWindow::onSmartInputSubmitted);
  connect(originalToggle_, &QPushButton::clicked, this, &MainWindow::onToggleOriginal);
  connect(generateButton_, &QPushButton::clicked, this, &MainWindow::onSmartInputSubmitted);
  connect(clear_, &QPushButton::clicked, this, &MainWindow::onClearInput);
  connect(specialButton_, &QPushButton::clicked, this, &MainWindow::onSpecialRequested);
}

void MainWindow::setupMoreMenu() {
  moreMenu_ = new QMenu(this);
  const QList<QPair<QString, vynx::PayloadType>> items = {
      {I18n::tr("SMS message"), vynx::PayloadType::Sms},
      {I18n::tr("Contact (vCard)"), vynx::PayloadType::VCard},
      {I18n::tr("Calendar event"), vynx::PayloadType::Event},
      {I18n::tr("2FA code (TOTP)"), vynx::PayloadType::Totp},
      {I18n::tr("Cryptocurrency (Crypto)"), vynx::PayloadType::Crypto},
      {I18n::tr("Bank payment / SBP"), vynx::PayloadType::Payment},
      {I18n::tr("Geolocation (Geo)"), vynx::PayloadType::Geo},
  };
  for (const auto &entry : items) {
    moreMenu_->addAction(entry.first, this, [this, entry] {
      setSpecialKind(entry.second);
      morePill_->setChecked(true);
    });
  }

  connect(morePill_, &QPushButton::clicked, this, [this] {
    if (moreMenu_) {
      moreMenu_->exec(morePill_->mapToGlobal(QPoint(0, morePill_->height())));
    }
  });
}

void MainWindow::buildPreview(QWidget *pane) {
  auto *layout = new QVBoxLayout(pane);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *previewCard = new QFrame(pane);
  previewCard->setProperty("role", QStringLiteral("card"));
  auto *cardLayout = new QVBoxLayout(previewCard);
  cardLayout->setContentsMargins(Tokens::SpaceMedium, Tokens::SpaceMedium,
                                 Tokens::SpaceMedium, Tokens::SpaceMedium);
  cardLayout->setSpacing(Tokens::SpaceSmall);

  preview_ = new QrPreview(previewCard);
  connect(preview_, &QrPreview::logoDropped, this, &MainWindow::onLogoDropped);
  cardLayout->addWidget(preview_, 1);

  auto *actions = new QHBoxLayout;
  actions->setSpacing(Tokens::SpaceSmall);

  save_ = new QPushButton(I18n::tr("Save") + QStringLiteral(" ▾"), previewCard);
  save_->setProperty("accent", true);
  save_->setAccessibleName(QStringLiteral("Save QR code"));
  save_->setEnabled(false);
  save_->setCursor(Qt::PointingHandCursor);
  actions->addWidget(save_, 1);

  copy_ = new QPushButton(I18n::tr("Copy"), previewCard);
  copy_->setAccessibleName(QStringLiteral("Copy QR code image to the clipboard"));
  copy_->setEnabled(false);
  copy_->setCursor(Qt::PointingHandCursor);
  actions->addWidget(copy_, 1);

  cardLayout->addLayout(actions);
  layout->addWidget(previewCard);

  connect(save_, &QPushButton::clicked, this, &MainWindow::onSaveAs);
  connect(copy_, &QPushButton::clicked, this, &MainWindow::onCopy);
}

void MainWindow::wireShortcuts() {
  auto *copyShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+C")), this);
  copyShortcut->setContext(Qt::WindowShortcut);
  connect(copyShortcut, &QShortcut::activated, this, [this] {
    for (QWidget *focus = QApplication::focusWidget(); focus && focus != this;
         focus = focus->parentWidget()) {
      auto *edit = qobject_cast<QLineEdit *>(focus);
      if (auto *spin = qobject_cast<QAbstractSpinBox *>(focus)) {
        edit = spin->findChild<QLineEdit *>();
      } else if (auto *combo = qobject_cast<QComboBox *>(focus)) {
        edit = combo->lineEdit();
      }
      if (edit) {
        edit->copy();
        return;
      }
      if (auto *plainText = qobject_cast<QPlainTextEdit *>(focus)) {
        plainText->copy();
        return;
      }
      if (auto *richText = qobject_cast<QTextEdit *>(focus)) {
        richText->copy();
        return;
      }
    }
    onCopy();
  });

  auto *saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
  connect(saveShortcut, &QShortcut::activated, this, &MainWindow::onSave);

  auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
  connect(settingsShortcut, &QShortcut::activated, this, &MainWindow::onOpenSettings);

  auto *snipShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), this);
  connect(snipShortcut, &QShortcut::activated, this, &MainWindow::onStartScreenSnipping);

  auto *clipScanShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")), this);
  connect(clipScanShortcut, &QShortcut::activated, this, &MainWindow::onScanFromClipboard);

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
  event->accept();
}

void MainWindow::setComposerMode(bool smart) {
  smartMode_ = smart;
  composerStack_->setCurrentIndex(smart ? 0 : 1);
  if (smart) {
    if (!textPill_->isChecked()) {
      urlPill_->setChecked(true);
    }
  }
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
    const vynx::SmartPayload smart = vynx::smart_payload(rust::Str(text));
    detected_ = smart.detected;
    changesInput_ = smart.payload.kind != vynx::PayloadType::Text &&
        (smart.analysis.changes_input || inputText_ != trimmed);
    detectedPayload_ = smart.payload;
    detectedKind_ = contentKindLabel(smart.analysis.kind);
    normalizationNotice_ = vynxq::toQString(smart.analysis.normalization);
    useOriginalText_ = false;

    if (leadingIconLabel_) {
      if (smart.analysis.kind == vynx::ContentKind::Url) {
        leadingIconLabel_->setText(QStringLiteral("🔗"));
      } else {
        leadingIconLabel_->setText(QStringLiteral("T"));
      }
    }
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
  if (kind == vynx::PayloadType::Wifi) {
    wifiPill_->setChecked(true);
  } else if (kind == vynx::PayloadType::Email) {
    emailPill_->setChecked(true);
  } else if (kind == vynx::PayloadType::Phone) {
    phonePill_->setChecked(true);
  } else {
    morePill_->setChecked(true);
  }
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
  showOriginalToggle(changesInput_, useOriginalText_);
}

void MainWindow::showOriginalToggle(bool visible, bool showingOriginal) {
  originalToggle_->setVisible(visible);
  originalToggle_->setText(showingOriginal ? QStringLiteral("Use detected %1").arg(detectedKind_)
                                           : QStringLiteral("Use original text"));
  chipRow_->setVisible(true);
}

vynx::Payload MainWindow::currentPayload() const {
  if (!smartMode_) {
    return forms_->payload();
  }
  if (useOriginalText_ || detectedPayload_.kind == vynx::PayloadType::Text) {
    return vynx::original_text_payload(rust::Str(inputText_.toStdString()));
  }
  return detectedPayload_;
}

vynx::RenderOptions MainWindow::currentOptions(std::uint32_t sizePx) const {
  vynx::RenderOptions options = customize_->options(sizePx);
  if (styleSelector_) {
    options.module_style = styleSelector_->selectedStyle();
  }
  options.foreground = rust::String(qrColor_.name().toStdString());
  options.background = rust::String(bgColor_.name().toStdString());
  return options;
}

void MainWindow::refresh() {
  const bool structuredAndIncomplete = !smartMode_ && !forms_->isComplete();
  const bool smartAndEmpty = smartMode_ && inputText_.trimmed().isEmpty();

  if (structuredAndIncomplete || smartAndEmpty) {
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
    const vynx::Bitmap bitmap =
        vynx::generate_bitmap(currentPayload(), currentOptions(customize_->exportSize()));
    const QImage image(reinterpret_cast<const uchar *>(bitmap.pixels.data()),
                       static_cast<int>(bitmap.width), static_cast<int>(bitmap.height),
                       static_cast<qsizetype>(bitmap.width * 4), QImage::Format_RGBA8888);
    QApplication::clipboard()->setImage(image.copy());
    copy_->setText(QStringLiteral("✓ ") + I18n::tr("Copied!"));
    QTimer::singleShot(1400, this, [this] { copy_->setText(I18n::tr("Copy")); });
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
  }
}

void MainWindow::onSave() { saveToDisk(); }

void MainWindow::onSaveAs() {
  if (current_.width == 0) {
    return;
  }
  QMenu menu(this);
  menu.addAction(I18n::tr("PNG image"), this, [this] {
    saveToDisk(vynx::ExportFormat::Png);
  });
  menu.addAction(I18n::tr("SVG vector"), this, [this] {
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
  applySettings(dialog.settings());
}

void MainWindow::applySettings(const vynx::Settings &settings) {
  const bool langChanged = (settings_.language != settings.language);
  settings_ = settings;
  customize_->applySettings(settings);
  if (styleSelector_) {
    styleSelector_->setStyle(settings.default_module_style);
  }
  updateColorSwatches();
  applyTheme();
  if (langChanged) {
    if (settings_.language == vynx::AppLanguage::Ru) {
      I18n::instance().setLanguage(I18n::Language::Russian);
    } else if (settings_.language == vynx::AppLanguage::Uk) {
      I18n::instance().setLanguage(I18n::Language::Ukrainian);
    } else {
      I18n::instance().setLanguage(I18n::Language::English);
    }
    retranslateUi();
  }
  try {
    vynx::save_settings(settings);
  } catch (const rust::Error &error) {
    showToast(vynxq::errorMessage(error));
  }
}

void MainWindow::onThemeToggle() {
  const bool isCurrentlyDark =
      settings_.theme == vynx::ThemeMode::Dark ||
      (settings_.theme == vynx::ThemeMode::System && PlatformIntegration::systemPrefersDarkMode());
  settings_.theme = isCurrentlyDark ? vynx::ThemeMode::Light : vynx::ThemeMode::Dark;
  try {
    vynx::save_settings(settings_);
  } catch (...) {}
  applyTheme();
}

void MainWindow::onStyleChanged(vynx::ModuleStyle style) {
  Q_UNUSED(style);
  ++generation_;
  refresh();
}

void MainWindow::onPickColor(bool foreground) {
  const QColor current = foreground ? qrColor_ : bgColor_;
  const QString title = foreground ? I18n::tr("Select QR Color") : I18n::tr("Select Background Color");
  const QColor chosen = QColorDialog::getColor(current, this, title);
  if (chosen.isValid()) {
    if (foreground) {
      qrColor_ = chosen;
    } else {
      bgColor_ = chosen;
    }
    updateColorSwatches();
    ++generation_;
    refresh();
  }
}

void MainWindow::updateColorSwatches() {
  if (fgColorButton_) {
    fgColorButton_->setIcon(QIcon(makeColorSwatchPixmap(qrColor_)));
    fgColorButton_->setText(qrColor_.name().toUpper());
  }
  if (bgColorButton_) {
    bgColorButton_->setIcon(QIcon(makeColorSwatchPixmap(bgColor_)));
    bgColorButton_->setText(bgColor_.name().toUpper());
  }
}

void MainWindow::retranslateUi() {
  setWindowTitle(I18n::tr("VYNX QR"));
  if (appTitleLabel_) appTitleLabel_->setText(I18n::tr("QR Generator"));
  if (themeToggle_) themeToggle_->setToolTip(I18n::tr("Toggle theme"));
  if (scanButton_) scanButton_->setText(QStringLiteral("📷  ") + I18n::tr("Scan"));
  if (aboutButton_) aboutButton_->setText(I18n::tr("About"));
  if (settingsButton_) settingsButton_->setText(I18n::tr("Settings"));

  if (composerHeading_) composerHeading_->setText(I18n::tr("Create QR code"));
  if (composerSubtitle_) {
    composerSubtitle_->setText(
        I18n::tr("Convert links, text, email, or phone numbers into QR codes"));
  }

  if (urlPill_) urlPill_->setText(QStringLiteral("🔗 ") + I18n::tr("Link"));
  if (textPill_) textPill_->setText(QStringLiteral("T ") + I18n::tr("Text"));
  if (emailPill_) emailPill_->setText(QStringLiteral("✉ ") + I18n::tr("Email"));
  if (phonePill_) phonePill_->setText(QStringLiteral("📞 ") + I18n::tr("Phone"));
  if (wifiPill_) wifiPill_->setText(QStringLiteral("📶 ") + I18n::tr("Wi-Fi"));
  if (morePill_) morePill_->setText(QStringLiteral("••• ") + I18n::tr("More") + QStringLiteral(" ▾"));

  if (styleHeading_) styleHeading_->setText(I18n::tr("QR Code Style"));
  if (styleSelector_) styleSelector_->retranslateUi();
  if (fgColorLabel_) fgColorLabel_->setText(I18n::tr("QR Color"));
  if (bgColorLabel_) bgColorLabel_->setText(I18n::tr("Background Color"));

  if (generateButton_) generateButton_->setText(I18n::tr("Generate QR code"));
  if (clear_) clear_->setText(I18n::tr("Clear"));
  if (copy_) copy_->setText(I18n::tr("Copy"));
  if (save_) save_->setText(I18n::tr("Save") + QStringLiteral(" ▾"));
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
  PlatformIntegration::setApplicationPalette(widgetPalette);
  qApp->setStyleSheet(styleSheetFor(palette));
  PlatformIntegration::applyWindowChrome(reinterpret_cast<void *>(winId()), palette.isDark());

  if (themeToggle_) {
    themeToggle_->setText(palette.isDark() ? QStringLiteral("☀️") : QStringLiteral("🌙"));
    themeToggle_->setToolTip(palette.isDark() ? I18n::tr("Light") : I18n::tr("Dark"));
  }
}

void MainWindow::onOpenAbout() {
  AboutDialog dialog(this, autoUpdater_);
  dialog.exec();
}

void MainWindow::onUpdateAvailable(const UpdateInfo &info) {
  UpdateDialog dialog(info, autoUpdater_, this);
  dialog.exec();
}

void MainWindow::readClipboardOnce() {
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
      {I18n::tr("Wi-Fi network"), vynx::PayloadType::Wifi},
      {I18n::tr("Contact (vCard)"), vynx::PayloadType::VCard},
      {I18n::tr("Email message"), vynx::PayloadType::Email},
      {I18n::tr("Phone number"), vynx::PayloadType::Phone},
      {I18n::tr("SMS message"), vynx::PayloadType::Sms},
      {I18n::tr("Geolocation (Geo)"), vynx::PayloadType::Geo},
      {I18n::tr("Calendar event"), vynx::PayloadType::Event},
      {I18n::tr("2FA code (TOTP)"), vynx::PayloadType::Totp},
      {I18n::tr("Cryptocurrency (Crypto)"), vynx::PayloadType::Crypto},
      {I18n::tr("Bank payment / SBP"), vynx::PayloadType::Payment},
  };
  for (const auto &entry : kinds) {
    menu.addAction(entry.first, this, [this, entry] {
      setSpecialKind(entry.second);
    });
  }
  if (specialButton_) {
    menu.exec(specialButton_->mapToGlobal(QPoint(0, 0)));
  }
}

void MainWindow::onScanMenuRequested() {
  QMenu menu(this);
  menu.addAction(I18n::tr("📷  Screen capture (Ctrl+Shift+S)"), this,
                 &MainWindow::onStartScreenSnipping);
  menu.addAction(I18n::tr("📁  Scan from file..."), this,
                 &MainWindow::onScanImageFile);
  menu.addAction(I18n::tr("📋  Scan from clipboard (Ctrl+Shift+V)"), this,
                 &MainWindow::onScanFromClipboard);
  if (scanButton_) {
    menu.exec(scanButton_->mapToGlobal(QPoint(0, scanButton_->height())));
  }
}

void MainWindow::onStartScreenSnipping() {
  if (snipper_) {
    snipper_->startSnipping();
  }
}

void MainWindow::onScanImageFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, I18n::tr("Select image with QR code"), QString(),
      I18n::tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
  if (path.isEmpty()) {
    return;
  }
  QImage img(path);
  if (img.isNull()) {
    showToast(I18n::tr("Failed to open image"));
    return;
  }
  img = img.convertToFormat(QImage::Format_RGBA8888);
  rust::Slice<const uint8_t> slice(img.constBits(), static_cast<size_t>(img.sizeInBytes()));
  try {
    const rust::String result = vynx::decode_qr_rgba(slice, static_cast<uint32_t>(img.width()),
                                                      static_cast<uint32_t>(img.height()));
    const QString text = vynxq::toQString(result);
    if (text.isEmpty()) {
      showToast(I18n::tr("QR code not found in image"));
      return;
    }
    onQrDecoded(text);
  } catch (const rust::Error &) {
    showToast(I18n::tr("QR code not found or corrupted"));
  }
}

void MainWindow::onScanFromClipboard() {
  const QClipboard *clipboard = QApplication::clipboard();
  const QImage img = clipboard->image();
  if (img.isNull()) {
    showToast(I18n::tr("No image in clipboard"));
    return;
  }
  QImage formatted = img.convertToFormat(QImage::Format_RGBA8888);
  rust::Slice<const uint8_t> slice(formatted.constBits(), static_cast<size_t>(formatted.sizeInBytes()));
  try {
    const rust::String result = vynx::decode_qr_rgba(slice, static_cast<uint32_t>(formatted.width()),
                                                      static_cast<uint32_t>(formatted.height()));
    const QString text = vynxq::toQString(result);
    if (text.isEmpty()) {
      showToast(I18n::tr("QR code not found in image"));
      return;
    }
    onQrDecoded(text);
  } catch (const rust::Error &) {
    showToast(I18n::tr("QR code not found in clipboard"));
  }
}

void MainWindow::onQrDecoded(const QString &text) {
  auto *dialog = new ScanResultDialog(text, this);
  connect(dialog, &ScanResultDialog::insertRequested, this, [this](const QString &content) {
    setComposerMode(true);
    inputField_->setText(content);
    inputText_ = content;
    onSmartInputChanged();
    showToast(I18n::tr("Inserted into generator"));
  });
  dialog->exec();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
    event->acceptProposedAction();
  } else {
    QMainWindow::dragEnterEvent(event);
  }
}

void MainWindow::dropEvent(QDropEvent *event) {
  if (event->mimeData()->hasImage()) {
    const QImage img = qvariant_cast<QImage>(event->mimeData()->imageData());
    if (!img.isNull()) {
      QImage formatted = img.convertToFormat(QImage::Format_RGBA8888);
      rust::Slice<const uint8_t> slice(formatted.constBits(), static_cast<size_t>(formatted.sizeInBytes()));
      try {
        const rust::String result = vynx::decode_qr_rgba(slice, static_cast<uint32_t>(formatted.width()),
                                                          static_cast<uint32_t>(formatted.height()));
        const QString text = vynxq::toQString(result);
        if (!text.isEmpty()) {
          onQrDecoded(text);
          event->acceptProposedAction();
          return;
        }
      } catch (...) {}
    }
  } else if (event->mimeData()->hasUrls()) {
    for (const QUrl &url : event->mimeData()->urls()) {
      const QString path = url.toLocalFile();
      QImage img(path);
      if (!img.isNull()) {
        img = img.convertToFormat(QImage::Format_RGBA8888);
        rust::Slice<const uint8_t> slice(img.constBits(), static_cast<size_t>(img.sizeInBytes()));
        try {
          const rust::String result = vynx::decode_qr_rgba(slice, static_cast<uint32_t>(img.width()),
                                                            static_cast<uint32_t>(img.height()));
          const QString text = vynxq::toQString(result);
          if (!text.isEmpty()) {
            onQrDecoded(text);
            event->acceptProposedAction();
            return;
          }
        } catch (...) {}
      }
    }
  }
  QMainWindow::dropEvent(event);
}

void MainWindow::onBackToSmart() {
  urlPill_->setChecked(true);
  setComposerMode(true);
}

void MainWindow::onCustomizeToggled() {
  customizeOpen_ = !customizeOpen_;
  if (customizeButton_) {
    customizeButton_->setChecked(customizeOpen_);
  }
  if (customize_) {
    customize_->setVisible(customizeOpen_);
  }
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
    const vynx::LogoAsset asset = vynx::load_logo(vynxq::toRust(path));
    vynx::Logo logo;
    logo.name = vynxq::toRust(QString::fromStdString(std::string(asset.name.data(), asset.name.size())));
    logo.data = readFile(path);
    if (logo.data.empty()) {
      showError(QStringLiteral("That image could not be read."));
      return;
    }
    customize_->setLogo(logo);
    if (!customizeOpen_) {
      onCustomizeToggled();
    }
    showToast(QStringLiteral("Logo added"));
  } catch (const rust::Error &error) {
    showError(vynxq::errorMessage(error));
  }
}
