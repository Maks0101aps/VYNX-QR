#include "SetupWindow.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QShortcut>
#include <QVBoxLayout>
#include "i18n/I18n.h"

namespace {
QLabel *label(const QString &text, const char *name, Qt::Alignment alignment = Qt::AlignLeft) {
  auto *widget = new QLabel(text);
  widget->setObjectName(QString::fromLatin1(name));
  widget->setAlignment(alignment);
  widget->setWordWrap(true);
  widget->setTextFormat(Qt::PlainText);
  return widget;
}
QLabel *logo() {
  auto *widget = new QLabel;
  widget->setPixmap(QPixmap(QStringLiteral(":/icons/vynx-qr.png")).scaled(88, 88, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  widget->setAlignment(Qt::AlignCenter);
  auto *shadow = new QGraphicsDropShadowEffect(widget);
  shadow->setColor(QColor(20, 95, 255, 75));
  shadow->setBlurRadius(38);
  shadow->setOffset(0, 7);
  widget->setGraphicsEffect(shadow);
  return widget;
}
}

SetupWindow::SetupWindow(const QString &payload, QWidget *parent)
    : QWidget(parent), backend_(payload, this) {
  I18n::instance().setLanguage(I18n::Language::English);
  setWindowTitle(I18n::tr("VYNX QR Setup"));
  resize(760, 510);
  setMinimumSize(720, 500);
  setObjectName(QStringLiteral("setup"));
  setStyleSheet(QStringLiteral(R"(
    QWidget#setup { background: #0b1321; color: #edf3ff; }
    QDialog, QMessageBox { background: #101e34; }
    QTextEdit { background: #0b1321; color: #edf3ff; }
    QWidget { font-family: "Segoe UI", "Noto Sans", sans-serif; font-size: 13px; color: #edf3ff; }
    QFrame#rail { background: #101e34; border-right: 1px solid #1d2d45; }
    QLabel { background: transparent; }
    QLabel#brand { font-size: 15px; font-weight: 600; }
    QLabel#step { color: #97a8c2; padding: 12px 14px; border-radius: 7px; }
    QLabel#step[active="true"] { color: #ffffff; background: #183764; font-weight: 600; }
    QLabel#step[done="true"] { color: #82afff; }
    QLabel#title { font-size: 25px; font-weight: 600; }
    QLabel#body, QLabel#destination { color: #9eafc7; font-size: 13px; }
    QLabel#footnote { color: #93a3bb; font-size: 11px; }
    QLabel#error { color: #ffb4b4; font-size: 12px; }
    QLabel#success { color: #8eb5ff; font-size: 38px; }
    QLineEdit { background: #101e32; border: 1px solid #3c506e; border-radius: 6px; padding: 10px; selection-background-color: #1761f5; }
    QLineEdit:focus { border: 1px solid #6da1ff; }
    QComboBox { background: #101e32; border: 1px solid #3c506e; border-radius: 6px; padding: 6px 10px; color: #edf3ff; }
    QComboBox:focus { border: 1px solid #6da1ff; }
    QComboBox QAbstractItemView { background: #101e32; color: #edf3ff; selection-background-color: #1761f5; }
    QPushButton { border: 1px solid #34465f; border-radius: 6px; padding: 10px 18px; background: #142238; }
    QPushButton:hover { background: #203756; border-color: #6583ac; }
    QPushButton:focus { border: 2px solid #9cc0ff; padding: 9px 17px; }
    QPushButton#next { background: #1761f5; border-color: #1761f5; font-weight: 600; min-width: 142px; }
    QPushButton#next:hover { background: #347bff; }
    QPushButton#next:focus { border-color: #bed5ff; }
    QPushButton:disabled { color: #75849a; background: #19263b; border-color: #26364d; }
    QPushButton#cancel { border-color: transparent; background: transparent; color: #9eafc7; }
    QPushButton#cancel:hover { color: white; background: #142238; }
    QProgressBar { border: 0; border-radius: 4px; background: #1e2d43; height: 8px; text-align: center; }
    QProgressBar::chunk { border-radius: 4px; background: #2774ff; }
    QCheckBox { spacing: 9px; color: #c6d3e7; }
    QCheckBox:focus { color: white; }
  )"));
  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  auto *rail = new QFrame;
  rail->setObjectName(QStringLiteral("rail"));
  rail->setFixedWidth(202);
  auto *railLayout = new QVBoxLayout(rail);
  railLayout->setContentsMargins(20, 29, 20, 22);
  railLayout->setSpacing(6);
  auto *brand = new QHBoxLayout;
  auto *smallIcon = new QLabel;
  smallIcon->setPixmap(QPixmap(QStringLiteral(":/icons/vynx-qr.png")).scaled(27, 27, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  brand->addWidget(smallIcon);
  brand->addSpacing(5);
  brandLabel_ = label(QStringLiteral("VYNX QR"), "brand");
  brand->addWidget(brandLabel_, 1);
  railLayout->addLayout(brand);
  railLayout->addSpacing(16);

  langCombo_ = new QComboBox;
  langCombo_->addItem(QStringLiteral("English"));
  langCombo_->addItem(QStringLiteral("Русский"));
  langCombo_->addItem(QStringLiteral("Українська"));
  langCombo_->setCurrentIndex(0);
  railLayout->addWidget(langCombo_);
  railLayout->addSpacing(18);

  const QStringList names = {I18n::tr("Welcome"), I18n::tr("Folder"),
                             I18n::tr("Installing"), I18n::tr("Completed")};
  for (int i = 0; i < 4; ++i) {
    steps_[i] = label(names[i], "step");
    railLayout->addWidget(steps_[i]);
  }
  railLayout->addStretch();
  railLayout->addWidget(label(QStringLiteral("VYNX TECHNOLOGY"), "footnote"));
  root->addWidget(rail);
  auto *content = new QVBoxLayout;
  content->setContentsMargins(34, 28, 34, 20);
  content->setSpacing(15);
  pages_ = new QStackedWidget;
  auto *welcome = new QWidget;
  auto *welcomeLayout = new QVBoxLayout(welcome);
  welcomeLayout->setContentsMargins(0, 0, 0, 0);
  welcomeLayout->addStretch();
  welcomeLayout->addWidget(logo());
  welcomeLayout->addSpacing(17);
  welcomeTitle_ = label(I18n::tr("Welcome to\nVYNX QR"), "title", Qt::AlignCenter);
  welcomeLayout->addWidget(welcomeTitle_);
  welcomeLayout->addSpacing(6);
  welcomeSub_ = label(I18n::tr("Fast. Private. Cross-platform."), "body", Qt::AlignCenter);
  welcomeLayout->addWidget(welcomeSub_);
  welcomeLayout->addSpacing(16);
  welcomeDesc_ = label(I18n::tr("Create and scan QR codes.\nNo account or internet connection required."), "body", Qt::AlignCenter);
  welcomeLayout->addWidget(welcomeDesc_);
  welcomeLayout->addStretch();
  pages_->addWidget(welcome);

  auto *folderPage = new QWidget;
  auto *folderLayout = new QVBoxLayout(folderPage);
  folderLayout->setContentsMargins(0, 22, 0, 0);
  folderTitle_ = label(I18n::tr("Where to install\nVYNX QR?"), "title");
  folderLayout->addWidget(folderTitle_);
  folderLayout->addSpacing(8);
  folderSub_ = label(I18n::tr("The application will be installed for your user only.\nAdministrator rights are not required."), "body");
  folderLayout->addWidget(folderSub_);
  folderLayout->addSpacing(23);
  folderLabel_ = label(I18n::tr("Installation folder"), "brand");
  folderLayout->addWidget(folderLabel_);
  folder_ = new QLineEdit(QDir::toNativeSeparators(SetupBackend::defaultDirectory()));
  folder_->setObjectName(QStringLiteral("folder"));
  folder_->setAccessibleName(I18n::tr("Installation folder"));
  folderLayout->addWidget(folder_);
  browseBtn_ = new QPushButton(I18n::tr("Browse…"));
  folderLayout->addWidget(browseBtn_, 0, Qt::AlignLeft);
  pathError_ = label({}, "error");
  folderLayout->addWidget(pathError_);
  folderLayout->addStretch();
  licenseFootnote_ = label(I18n::tr("By continuing, you accept the VYNX QR license.\nLicense texts are included in the package."), "footnote");
  folderLayout->addWidget(licenseFootnote_);
  licenseBtn_ = new QPushButton(I18n::tr("View license"));
  folderLayout->addWidget(licenseBtn_, 0, Qt::AlignLeft);
  connect(licenseBtn_, &QPushButton::clicked, this, [this] {
    QFile file(QStringLiteral(":/setup/LICENSE"));
    if (file.open(QIODevice::ReadOnly)) {
      QMessageBox box(QMessageBox::Information, I18n::tr("VYNX QR License"),
                      I18n::tr("VYNX QR · Terms of Use"), QMessageBox::Ok, this);
      box.setDetailedText(QString::fromUtf8(file.readAll()));
      box.exec();
    }
  });
  pages_->addWidget(folderPage);
  auto *installPage = new QWidget;
  auto *installLayout = new QVBoxLayout(installPage);
  installLayout->setContentsMargins(0, 0, 0, 0);
  installLayout->addStretch();
  installLayout->addWidget(logo());
  installLayout->addSpacing(22);
  status_ = label(I18n::tr("Installing VYNX QR"), "title", Qt::AlignCenter);
  installLayout->addWidget(status_);
  installLayout->addSpacing(12);
  progress_ = new QProgressBar;
  progress_->setTextVisible(false);
  progress_->setAccessibleName(I18n::tr("Installing VYNX QR"));
  installLayout->addWidget(progress_);
  destination_ = label({}, "destination", Qt::AlignCenter);
  installLayout->addWidget(destination_);
  error_ = label({}, "error", Qt::AlignCenter);
  error_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  installLayout->addWidget(error_);
  installLayout->addStretch();
  pages_->addWidget(installPage);
  auto *finishPage = new QWidget;
  auto *finishLayout = new QVBoxLayout(finishPage);
  finishLayout->setContentsMargins(0, 0, 0, 0);
  finishLayout->addStretch();
  finishLayout->addWidget(label(QStringLiteral("✓"), "success", Qt::AlignCenter));
  finishLayout->addSpacing(16);
  finishTitle_ = label(I18n::tr("All done!"), "title", Qt::AlignCenter);
  finishLayout->addWidget(finishTitle_);
  finishLayout->addSpacing(8);
  finishSub_ = label(I18n::tr("VYNX QR has been installed.\nThe shortcut is in your applications menu."), "body", Qt::AlignCenter);
  finishLayout->addWidget(finishSub_);
  finishLayout->addSpacing(24);
  launch_ = new QCheckBox(I18n::tr("Launch VYNX QR"));
  launch_->setShortcut(QKeySequence(QStringLiteral("Alt+R")));
  launch_->setChecked(true);
  finishLayout->addWidget(launch_, 0, Qt::AlignHCenter);
  finishLayout->addStretch();
  pages_->addWidget(finishPage);
  content->addWidget(pages_, 1);
  auto *buttons = new QHBoxLayout;
  back_ = new QPushButton(I18n::tr("Back"));
  back_->setObjectName(QStringLiteral("back"));
  cancel_ = new QPushButton(I18n::tr("Cancel"));
  cancel_->setObjectName(QStringLiteral("cancel"));
  next_ = new QPushButton;
  next_->setObjectName(QStringLiteral("next"));
  next_->setAutoDefault(true);
  buttons->addWidget(back_);
  buttons->addWidget(cancel_);
  buttons->addStretch();
  buttons->addWidget(next_);
  content->addLayout(buttons);
  versionLabel_ = label(I18n::tr("Version %1").arg(QStringLiteral(VYNX_VERSION)), "footnote", Qt::AlignRight);
  content->addWidget(versionLabel_);
  root->addLayout(content, 1);
  connect(next_, &QPushButton::clicked, this, &SetupWindow::advance);
  connect(new QShortcut(QKeySequence(Qt::Key_Return), this), &QShortcut::activated, this,
          [this] { if (next_->isEnabled()) next_->click(); });
  connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this, &QWidget::close);
  connect(new QShortcut(QKeySequence(QStringLiteral("Alt+D")), this), &QShortcut::activated, this,
          [this] { if (step_ == 1) { folder_->setFocus(); folder_->selectAll(); } });
  connect(back_, &QPushButton::clicked, this, [this] { setStep(step_ == 2 ? 1 : 0); });
  connect(cancel_, &QPushButton::clicked, this, &QWidget::close);
  connect(browseBtn_, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getExistingDirectory(this, I18n::tr("Installation folder"), folder_->text());
    if (!path.isEmpty()) folder_->setText(QDir::toNativeSeparators(path));
  });
  connect(folder_, &QLineEdit::textChanged, this, [this] {
    const auto error = SetupBackend::pathError(folder_->text());
    pathError_->setText(error);
    if (step_ == 1) next_->setEnabled(error.isEmpty());
  });
  connect(langCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index == 1) {
      I18n::instance().setLanguage(I18n::Language::Russian);
    } else if (index == 2) {
      I18n::instance().setLanguage(I18n::Language::Ukrainian);
    } else {
      I18n::instance().setLanguage(I18n::Language::English);
    }
    retranslateUi();
  });
  connect(&backend_, &SetupBackend::progress, this, [this](int value) {
    progress_->setRange(0, value < 0 ? 0 : 100);
    if (value >= 0) progress_->setValue(value);
  });
  connect(&backend_, &SetupBackend::finished, this, [this](bool success, const QString &error) {
    busy_ = false;
    if (success) { setStep(3); return; }
    progress_->setRange(0, 100);
    progress_->setValue(0);
    status_->setText(I18n::tr("Installation interrupted"));
    error_->setText(error);
    next_->setText(I18n::tr("Retry"));
    next_->setEnabled(true);
    back_->show();
    cancel_->setEnabled(true);
  });
  setStep(0);
}

void SetupWindow::retranslateUi() {
  setWindowTitle(I18n::tr("VYNX QR Setup"));
  if (welcomeTitle_) welcomeTitle_->setText(I18n::tr("Welcome to\nVYNX QR"));
  if (welcomeSub_) welcomeSub_->setText(I18n::tr("Fast. Private. Cross-platform."));
  if (welcomeDesc_) welcomeDesc_->setText(I18n::tr("Create and scan QR codes.\nNo account or internet connection required."));
  if (folderTitle_) folderTitle_->setText(I18n::tr("Where to install\nVYNX QR?"));
  if (folderSub_) folderSub_->setText(I18n::tr("The application will be installed for your user only.\nAdministrator rights are not required."));
  if (folderLabel_) folderLabel_->setText(I18n::tr("Installation folder"));
  if (browseBtn_) browseBtn_->setText(I18n::tr("Browse…"));
  if (licenseFootnote_) licenseFootnote_->setText(I18n::tr("By continuing, you accept the VYNX QR license.\nLicense texts are included in the package."));
  if (licenseBtn_) licenseBtn_->setText(I18n::tr("View license"));
  if (finishTitle_) finishTitle_->setText(I18n::tr("All done!"));
  if (finishSub_) finishSub_->setText(I18n::tr("VYNX QR has been installed.\nThe shortcut is in your applications menu."));
  if (launch_) launch_->setText(I18n::tr("Launch VYNX QR"));
  if (back_) back_->setText(I18n::tr("Back"));
  if (cancel_) cancel_->setText(I18n::tr("Cancel"));
  if (versionLabel_) versionLabel_->setText(I18n::tr("Version %1").arg(QStringLiteral(VYNX_VERSION)));
  setStep(step_);
}

void SetupWindow::setStep(int step) {
  step_ = step;
  setWindowTitle(I18n::tr("VYNX QR Setup"));
  pages_->setCurrentIndex(step);
  const QStringList names = {I18n::tr("Welcome"), I18n::tr("Folder"),
                             I18n::tr("Installing"), I18n::tr("Completed")};
  for (int i = 0; i < 4; ++i) {
    steps_[i]->setText((i < step ? QStringLiteral("✓  ") : QString::number(i + 1) + QStringLiteral("  ")) + names[i]);
    steps_[i]->setProperty("active", i == step);
    steps_[i]->setProperty("done", i < step);
    steps_[i]->style()->unpolish(steps_[i]);
    steps_[i]->style()->polish(steps_[i]);
  }
  back_->setVisible(step == 1);
  cancel_->setVisible(step != 3);
  cancel_->setEnabled(!busy_);
  next_->setText(step == 1 ? I18n::tr("Install") : step == 3 ? I18n::tr("Finish") : I18n::tr("Continue  →"));
  next_->setEnabled(!busy_ && (step != 1 || SetupBackend::pathError(folder_->text()).isEmpty()));
  next_->setFocus();
}

void SetupWindow::advance() {
  if (step_ == 0) { setStep(1); return; }
  if (step_ == 3) {
    if (launch_->isChecked() && !QProcess::startDetached(backend_.executable(), {})) {
      QMessageBox::warning(this, QStringLiteral("VYNX QR"), I18n::tr("Could not start the application. Open VYNX QR from the applications menu."));
    }
    close(); return;
  }
  busy_ = true;
  setStep(2);
  status_->setText(I18n::tr("Installing VYNX QR"));
  error_->clear();
  destination_->setText(folder_->text());
  progress_->setRange(0, 0);
  backend_.install(folder_->text());
}
void SetupWindow::closeEvent(QCloseEvent *event) {
  if (busy_) { event->ignore(); return; }
  event->accept();
}
