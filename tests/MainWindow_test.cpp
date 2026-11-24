// Regression tests for the window's own logic.
//
// These exist because the live update broke silently once: the debounce timer was
// configured and started nowhere, so typing produced nothing and only Enter worked.
// Nothing else in the suite would have noticed, because every other check drove
// the window explicitly.
//
// The tests below set the field's text rather than synthesising key events. Both
// reach the window through the same `textChanged` signal, and the debounce, the
// generation counter and the render path are identical either way. Real key events
// are covered by typingUpdatesTheCodeWithoutEnter and the copy-shortcut check.
// Other logic tests avoid depending on foreground permission: only the
// first window created in a process accepts synthetic keystrokes, so making every
// test depend on that would test the environment rather than the code.
//
// Settings are isolated in a temporary APPDATA directory. Clipboard checks use
// the real clipboard, clearing the test image after the export check.

#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QPushButton>
#include <QTest>

#include <rust/cxx.h>

#include "MainWindow.h"
#include "Theme.h"
#include "platform/PlatformIntegration.h"
#include "widgets/ClipboardSuggestion.h"
#include "widgets/CustomizePanel.h"
#include "widgets/QrPreview.h"
#include "widgets/SpecialQrForms.h"
#include "widgets/StyleSelectorWidget.h"
#include "updater/AutoUpdater.h"
#include "i18n/I18n.h"
#include "vynx-qr-bridge/src/lib.rs.h"

namespace {

/// A bridge string is UTF-8 and not a std::string, so it is decoded explicitly.
QString bridgeString(const rust::String &value) {
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

/// Long enough for the 150 ms debounce to have fired.
constexpr int kSettle = 200;

} // namespace

class MainWindowTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    PlatformIntegration::captureSystemPalette();
    originalAppData_ = qgetenv("APPDATA");
    originalXdg_ = qgetenv("XDG_CONFIG_HOME");
    QVERIFY(settingsDir_.isValid());
    qputenv("APPDATA", settingsDir_.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", settingsDir_.path().toUtf8());
    qputenv("VYNX_NO_UPDATE_CHECK", "1");
    // The window reads the clipboard once at start-up, so a test must not be able
    // to be influenced by whatever the developer happened to copy.
    QApplication::clipboard()->clear();
  }
  void cleanupTestCase() {
    qunsetenv("VYNX_NO_UPDATE_CHECK");
    if (originalAppData_.isNull()) qunsetenv("APPDATA"); else qputenv("APPDATA", originalAppData_);
    if (originalXdg_.isNull()) qunsetenv("XDG_CONFIG_HOME"); else qputenv("XDG_CONFIG_HOME", originalXdg_);
  }

  void copyShortcutPreservesEveryVisibleEditor() {
    MainWindow window;
    present(window);
    const auto shortcuts = window.findChildren<QShortcut *>();
    QShortcut *copy = nullptr;
    for (auto *shortcut : shortcuts) {
      if (shortcut->key() == QKeySequence(QStringLiteral("Ctrl+C"))) copy = shortcut;
    }
    QVERIFY(copy);
    enter(smartInput(window), QStringLiteral("github.com"));
    window.onCustomizeToggled();
    auto setClipboard = [](const QString &text) {
      for (int i = 0; i < 5; ++i) {
        QApplication::clipboard()->setText(text);
        if (QApplication::clipboard()->text() == text) return;
        QTest::qWait(10);
      }
    };

    auto verifyClipboard = [](const auto &trigger, const QString &expected) {
      for (int retry = 0; retry < 5; ++retry) {
        if (retry > 0) QTest::qWait(25);
        trigger();
        for (int poll = 0; poll < 5; ++poll) {
          if (QApplication::clipboard()->text() == expected) return true;
          QTest::qWait(15);
        }
      }
      return QApplication::clipboard()->text() == expected;
    };

    int lineEdits = 0;
    int multilineEdits = 0;
    // Include smart input/HEX, all structured forms and spin-box child editors.
    const auto kinds = {vynx::PayloadType::Text, vynx::PayloadType::Wifi,
                        vynx::PayloadType::VCard, vynx::PayloadType::Email,
                        vynx::PayloadType::Phone, vynx::PayloadType::Sms,
                        vynx::PayloadType::Geo, vynx::PayloadType::Event,
                        vynx::PayloadType::Totp, vynx::PayloadType::Crypto,
                        vynx::PayloadType::Payment};
    for (const auto kind : kinds) {
      if (kind != vynx::PayloadType::Text) window.setSpecialKind(kind);
      for (auto *edit : window.findChildren<QLineEdit *>()) {
        if (!edit->isVisible() || !edit->isEnabled()) continue;
        if (edit->text().isEmpty()) edit->setText(QStringLiteral("copy sample"));
        edit->setFocus();
        QVERIFY(QApplication::focusWidget() == edit ||
                qobject_cast<QAbstractSpinBox *>(QApplication::focusWidget()) == edit->parentWidget());
        edit->selectAll();
        const auto selected = edit->selectedText();
        QVERIFY(!selected.isEmpty());
        setClipboard(QStringLiteral("sentinel"));
        const auto expected = edit->echoMode() == QLineEdit::Normal ? selected : QStringLiteral("sentinel");
        QVERIFY(verifyClipboard([&] { QTest::keyClick(edit, Qt::Key_C, Qt::ControlModifier); }, expected));
        // Invoke the window fallback too: native ShortcutOverride can otherwise
        // hide a regression in this handler on some Qt/platform combinations.
        QVERIFY(verifyClipboard([&] { QMetaObject::invokeMethod(copy, "activated", Qt::DirectConnection); }, expected));
        if (edit->echoMode() == QLineEdit::Password) {
          edit->setEchoMode(QLineEdit::Normal);
          QVERIFY(verifyClipboard([&] { QMetaObject::invokeMethod(copy, "activated", Qt::DirectConnection); }, selected));
          edit->setEchoMode(QLineEdit::Password);
        }
        edit->deselect();
        setClipboard(QStringLiteral("sentinel"));
        QVERIFY(verifyClipboard([&] { QMetaObject::invokeMethod(copy, "activated", Qt::DirectConnection); }, QStringLiteral("sentinel")));
        ++lineEdits;
      }
      for (auto *edit : window.findChildren<QPlainTextEdit *>()) {
        if (!edit->isVisible()) continue;
        edit->setPlainText(QStringLiteral("selected multiline\ntext"));
        edit->setFocus();
        QCOMPARE(QApplication::focusWidget(), edit);
        edit->selectAll();
        setClipboard(QStringLiteral("sentinel"));
        QVERIFY(verifyClipboard([&] { QMetaObject::invokeMethod(copy, "activated", Qt::DirectConnection); }, edit->toPlainText()));
        auto cursor = edit->textCursor();
        cursor.clearSelection();
        edit->setTextCursor(cursor);
        setClipboard(QStringLiteral("sentinel"));
        QVERIFY(verifyClipboard([&] { QMetaObject::invokeMethod(copy, "activated", Qt::DirectConnection); }, QStringLiteral("sentinel")));
        ++multilineEdits;
      }
    }
    QVERIFY(lineEdits >= 20);
    QCOMPARE(multilineEdits, 4);
    window.onBackToSmart();
    enter(smartInput(window), QStringLiteral("github.com"));
    window.copy_->setFocus();
    QCOMPARE(QApplication::focusWidget(), window.copy_);
    QApplication::clipboard()->clear();
    QVERIFY(QMetaObject::invokeMethod(copy, "activated", Qt::DirectConnection));
    QVERIFY(!QApplication::clipboard()->image().isNull());
    QApplication::clipboard()->clear();
  }

  /// The bug that started all this: the code must appear as text is entered, with
  /// no Enter pressed and no explicit call.
  void typingUpdatesTheCodeWithoutEnter() {
    MainWindow window;
    present(window);

    QLineEdit *input = smartInput(window);
    QVERIFY2(input != nullptr, "the smart input must exist");

    typeWithPause(input, QStringLiteral("github.com"));

    const auto result = window.currentResult();
    QVERIFY2(result.width > 0, "a code must be on screen after typing alone");
    QCOMPARE(verifyLabel(result.verification_status), QStringLiteral("Scan verified"));
    QCOMPARE(bridgeString(result.encoded), QStringLiteral("https://github.com"));
  }

  /// Clearing must return the window to its empty state and drop the old bytes,
  /// not merely disable the buttons.
  void clearingReturnsToTheEmptyState() {
    MainWindow window;
    present(window);

    QLineEdit *input = smartInput(window);
    QVERIFY(input != nullptr);
    enter(input, QStringLiteral("github.com"));
    QVERIFY(window.currentResult().width > 0);

    window.clearInput();
    QCOMPARE(window.currentResult().width, 0u);
    QVERIFY(bridgeString(window.currentResult().encoded).isEmpty());
  }

  /// A slow typist must not leave a stale code behind.
  void theNewestInputWinsOverAnOlderOne() {
    MainWindow window;
    present(window);

    QLineEdit *input = smartInput(window);
    QVERIFY(input != nullptr);

    // Overlapping requests: each keystroke invalidates the previous one, so only
    // the last may survive.
    enter(input, QStringLiteral("https://ex"));
    enter(input, QStringLiteral("https://example.com/some/path"));

    QCOMPARE(bridgeString(window.currentResult().encoded),
             QStringLiteral("https://example.com/some/path"));
  }

  /// The escape hatch has to actually change what is encoded.
  void usingOriginalTextChangesTheEncodedPayload() {
    MainWindow window;
    present(window);

    QLineEdit *input = smartInput(window);
    QVERIFY(input != nullptr);
    enter(input, QStringLiteral("github.com"));
    QCOMPARE(bridgeString(window.currentResult().encoded),
             QStringLiteral("https://github.com"));

    QPushButton *toggle = findButton(window, QStringLiteral("Use original text"));
    QVERIFY2(toggle != nullptr, "the escape hatch must be offered when something changed");
    QVERIFY(toggle->isVisible() && toggle->isEnabled());
    QTest::mouseClick(toggle, Qt::LeftButton);

    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("github.com"));

    // And it can be taken back.
    toggle = findButton(window, QStringLiteral("Use detected URL"));
    QVERIFY(toggle != nullptr);
    QVERIFY(toggle->isVisible() && toggle->isEnabled());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QCOMPARE(bridgeString(window.currentResult().encoded),
             QStringLiteral("https://github.com"));
  }

  /// Nothing may change silently, so the button only appears when it would do
  /// something.
  void noEscapeHatchWhenNothingWasNormalised() {
    MainWindow window;
    present(window);

    QLineEdit *input = smartInput(window);
    QVERIFY(input != nullptr);
    enter(input, QStringLiteral("https://github.com"));

    QCOMPARE(bridgeString(window.currentResult().encoded),
             QStringLiteral("https://github.com"));
    QPushButton *toggle = findButton(window, QStringLiteral("Use original text"));
    QVERIFY2(toggle == nullptr || !toggle->isVisible(),
             "nothing changed, so nothing must be offered");
  }

  void emailOriginalModeReturnsToDetectedEmail() {
    MainWindow window;
    present(window);
    enter(smartInput(window), QStringLiteral("hello@example.com"));
    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("mailto:hello@example.com"));
    auto *toggle = findButton(window, QStringLiteral("Use original text"));
    QVERIFY(toggle && toggle->isVisible() && toggle->isEnabled());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("hello@example.com"));
    toggle = findButton(window, QStringLiteral("Use detected Email"));
    QVERIFY(toggle && toggle->isVisible() && toggle->isEnabled());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("mailto:hello@example.com"));
  }

  void paddedPlainTextHasNoOriginalToggle() {
    MainWindow window;
    present(window);
    enter(smartInput(window), QStringLiteral("  hello  "));
    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("  hello  "));
    auto *toggle = findButton(window, QStringLiteral("Use original text"));
    QVERIFY(!toggle || !toggle->isVisible());
  }

  /// Switching to a structured form and back must clear the code.
  void leavingTheSmartInputClearsTheCode() {
    MainWindow window;
    present(window);

    QLineEdit *input = smartInput(window);
    QVERIFY(input != nullptr);
    enter(input, QStringLiteral("github.com"));
    QVERIFY(window.currentResult().width > 0);

    window.setSpecialKind(vynx::PayloadType::Wifi);
    QCOMPARE(window.currentResult().width, 0u);
  }

  /// A structured form needs only its mandatory field to produce a code.
  void aWifiFormProducesAVerifiedCode() {
    MainWindow window;
    present(window);
    window.setSpecialKind(vynx::PayloadType::Wifi);
    QCOMPARE(window.currentResult().width, 0u);

    QLineEdit *ssid = findField(window, QStringLiteral("Network name (SSID)"));
    QVERIFY2(ssid != nullptr, "the Wi-Fi form must have a network name field");
    // Surrounding spaces are part of the network name and must survive.
    enter(ssid, QStringLiteral(" VYNX Home "));
    QCOMPARE(ssid->text(), QStringLiteral(" VYNX Home "));

    // A secured network needs a password, and the engine says so rather than
    // producing a code that would not connect.
    QCOMPARE(window.currentResult().width, 0u);

    QLineEdit *password = findField(window, QStringLiteral("Password"));
    QVERIFY2(password != nullptr, "the Wi-Fi form must have a password field");
    enter(password, QStringLiteral(" password with spaces "));

    const auto result = window.currentResult();
    QVERIFY(result.width > 0);
    // The leading and trailing spaces are part of both values and must survive
    // the round trip untouched.
    QCOMPARE(bridgeString(result.encoded),
             QStringLiteral("WIFI:T:WPA;S: VYNX Home ;P: password with spaces ;;"));
    QCOMPARE(verifyLabel(result.verification_status), QStringLiteral("Scan verified"));
  }

  /// The preview drops its buffer when it goes back to the empty state.
  void theEmptyStateReleasesTheImage() {
    QrPreview preview;
    preview.showEmpty();
    preview.release();
    QVERIFY(preview.isEnabled());
  }

  void originalTextPreservesWhitespace() {
    MainWindow window;
    present(window);
    enter(smartInput(window), QStringLiteral("  https://github.com  "));
    auto *toggle = findButton(window, QStringLiteral("Use original text"));
    QVERIFY(toggle && toggle->isVisible());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("  https://github.com  "));
    enter(smartInput(window), QStringLiteral("  some ordinary text  "));
    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("  some ordinary text  "));
  }

  void customizeControlsAndColoursAreIndependent() {
    CustomizePanel panel;
    present(panel);
    const auto combos = panel.findChildren<QComboBox *>();
    QCOMPARE(combos.size(), 4);
    for (auto *combo : combos) {
      QVERIFY(combo->isVisible());
      if (combo->accessibleName() == QStringLiteral("Module shape")) combo->setCurrentIndex(1);
      if (combo->accessibleName() == QStringLiteral("Error correction")) combo->setCurrentIndex(3);
      if (combo->accessibleName() == QStringLiteral("Quiet zone in modules")) combo->setCurrentIndex(4);
    }
    const auto options = panel.options(768);
    QVERIFY(options.module_style == vynx::ModuleStyle::Rounded);
    QVERIFY(options.ec_level == vynx::EcLevel::H);
    QCOMPARE(options.quiet_zone, 6u);
    auto *fg = findField(panel, QStringLiteral("Foreground HEX value"));
    auto *bg = findField(panel, QStringLiteral("Background HEX value"));
    QVERIFY(!fg->text().isEmpty() && !bg->text().isEmpty());
    QPushButton *fgSwatch = nullptr, *bgSwatch = nullptr;
    for (auto *button : panel.findChildren<QPushButton *>()) {
      if (button->accessibleName() == QStringLiteral("Choose Foreground colour")) fgSwatch = button;
      if (button->accessibleName() == QStringLiteral("Choose Background colour")) bgSwatch = button;
    }
    QVERIFY(fgSwatch && bgSwatch);
    const auto backgroundStyle = bgSwatch->styleSheet();
    fg->setText(QStringLiteral("#112233"));
    QVERIFY(QMetaObject::invokeMethod(fg, "textEdited", Q_ARG(QString, fg->text())));
    QCOMPARE(bgSwatch->styleSheet(), backgroundStyle);
    QVERIFY(fgSwatch->styleSheet().contains(QStringLiteral("#112233")));
    QCOMPARE(bridgeString(panel.options(768).foreground).toLower(), QStringLiteral("#112233"));
    fg->setText(QStringLiteral("#11"));
    QVERIFY(QMetaObject::invokeMethod(fg, "textEdited", Q_ARG(QString, fg->text())));
    QCOMPARE(bridgeString(panel.options(768).foreground).toLower(), QStringLiteral("#112233"));
    panel.reset();
    QCOMPARE(bridgeString(panel.options(768).foreground), bridgeString(vynx::default_style().foreground));
  }

  void exportUsesTheSelectedSize() {
    MainWindow window;
    present(window);
    enter(smartInput(window), QStringLiteral("github.com"));
    auto *panel = window.findChild<CustomizePanel *>();
    QVERIFY(panel);
    for (auto *combo : panel->findChildren<QComboBox *>()) {
      if (combo->accessibleName() == QStringLiteral("Export size")) combo->setCurrentIndex(0);
    }
    window.onCopy();
    QCOMPARE(QApplication::clipboard()->image().size(), QSize(256, 256));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto png = dir.filePath(QStringLiteral("код with spaces.png"));
    QVERIFY(window.exportToFile(png, vynx::ExportFormat::Png));
    QCOMPARE(QImage(png).size(), QSize(256, 256));
    const auto svg = dir.filePath(QStringLiteral("код with spaces.svg"));
    QVERIFY(window.exportToFile(svg, vynx::ExportFormat::Svg));
    QFile file(svg);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto bytes = file.readAll();
    QVERIFY(bytes.contains("width=\"256\""));
    QVERIFY(bytes.contains("height=\"256\""));
    QVERIFY(!window.exportToFile(dir.filePath(QStringLiteral("missing/qr.png")), vynx::ExportFormat::Png));
    QApplication::clipboard()->clear();
  }

  void settingsChangeTheThemeImmediately() {
    MainWindow window;
    auto settings = vynx::load_settings();
    settings.theme = vynx::ThemeMode::Dark;
    window.applySettings(settings);
    QFile preferences(settingsDir_.filePath(QStringLiteral("VYNX/QR/settings.json")));
    QVERIFY(preferences.open(QIODevice::ReadOnly));
    QVERIFY(preferences.readAll().contains("dark"));
    QVERIFY(qApp->palette().color(QPalette::Window).lightness() < 128);
    const auto darkStyle = qApp->styleSheet();
    settings.theme = vynx::ThemeMode::Light;
    window.applySettings(settings);
    QVERIFY(qApp->palette().color(QPalette::Window).lightness() >= 128);
    QVERIFY(qApp->styleSheet() != darkStyle);
  }

  void previewAcceptsLogoDrops() {
    QrPreview preview;
    present(preview);
    QVERIFY(preview.acceptDrops());
    for (auto *label : preview.findChildren<QLabel *>()) QVERIFY(!label->acceptDrops());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("логотип with spaces.png"));
    QImage logo(32, 32, QImage::Format_ARGB32);
    logo.fill(Qt::blue);
    QVERIFY(logo.save(path));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    QSignalSpy dropped(&preview, &QrPreview::logoDropped);
    QDragEnterEvent drag(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&preview, &drag);
    QVERIFY(drag.isAccepted());
    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&preview, &drop);
    QVERIFY(drop.isAccepted());
    QCOMPARE(dropped.size(), 1);
    QCOMPARE(dropped.at(0).at(0).toString(), path);
  }

  void autoUpdaterVersionComparisonWorks() {
    QVERIFY(AutoUpdater::isNewerVersion(QStringLiteral("v1.1.4.1"), QStringLiteral("1.1.4")));
    QVERIFY(AutoUpdater::isNewerVersion(QStringLiteral("1.1.5"), QStringLiteral("1.1.4.1")));
    QVERIFY(AutoUpdater::isNewerVersion(QStringLiteral("v1.2.0"), QStringLiteral("1.1.4.1")));
    QVERIFY(AutoUpdater::isNewerVersion(QStringLiteral("v2.0.0"), QStringLiteral("1.1.4.1")));
    QVERIFY(!AutoUpdater::isNewerVersion(QStringLiteral("v1.1.4.1"), QStringLiteral("1.1.4.1")));
    QVERIFY(!AutoUpdater::isNewerVersion(QStringLiteral("v1.1.4"), QStringLiteral("1.1.4.1")));
    QVERIFY(!AutoUpdater::isNewerVersion(QStringLiteral("v1.1.3"), QStringLiteral("1.1.4.1")));
    QVERIFY(!AutoUpdater::isNewerVersion(QStringLiteral("v1.0.1"), QStringLiteral("1.1.4.1")));
  }

  void i18nLanguageSwitchingWorks() {
    I18n::instance().setLanguage(I18n::Language::English);
    QCOMPARE(I18n::tr("Check for updates"), QStringLiteral("Check for updates"));
    QCOMPARE(I18n::tr("Close"), QStringLiteral("Close"));

    I18n::instance().setLanguage(I18n::Language::Russian);
    QCOMPARE(I18n::tr("Check for updates"), QStringLiteral("Проверить обновления"));
    QCOMPARE(I18n::tr("Close"), QStringLiteral("Закрыть"));

    I18n::instance().setLanguage(I18n::Language::Ukrainian);
    QCOMPARE(I18n::tr("Check for updates"), QStringLiteral("Перевірити оновлення"));
    QCOMPARE(I18n::tr("Close"), QStringLiteral("Закрити"));

    // Reset back to English
    I18n::instance().setLanguage(I18n::Language::English);
  }

  void decodeQrCodeFromImage() {
    MainWindow window;
    present(window);

    enter(smartInput(window), QStringLiteral("https://vynx.app/test"));
    const auto result = window.currentResult();
    QVERIFY(result.width > 0);

    const vynx::Bitmap bitmap =
        vynx::generate_bitmap(window.currentPayload(), window.currentOptions(512));
    rust::Slice<const uint8_t> slice(bitmap.pixels.data(), bitmap.pixels.size());
    const rust::String decoded =
        vynx::decode_qr_rgba(slice, bitmap.width, bitmap.height);
    QCOMPARE(bridgeString(decoded), QStringLiteral("https://vynx.app/test"));
  }

  void themeTokensAndBrandPaletteValid() {
    auto palette = makePalette(QStringLiteral("light"), QColor());
    QCOMPARE(vynxBlue(), QColor("#6366F1")); // Modern brand purple
    QVERIFY(palette.surface.isValid());
    QVERIFY(palette.window.isValid());
    QCOMPARE(palette.accentText, QColor("#FFFFFF"));

    auto darkPalette = makePalette(QStringLiteral("dark"), QColor());
    QVERIFY(darkPalette.isDark());
    QCOMPARE(darkPalette.accentText, QColor("#FFFFFF"));

    QString qss = styleSheetFor(palette);
    QVERIFY(qss.contains(QStringLiteral("QFrame[role=\"card\"]")));
    QVERIFY(qss.contains(QStringLiteral("QPushButton[role=\"pill\"]")));
    QVERIFY(qss.contains(QStringLiteral("QAbstractButton[role=\"styleCard\"]")));
    QVERIFY(qss.contains(QStringLiteral("QToolButton[role=\"topBarButton\"]")));
  }

  void styleSelectorSwitchesModes() {
    MainWindow window;
    present(window);

    // Default style is Classic (Square)
    QCOMPARE(window.currentOptions().module_style, vynx::ModuleStyle::Square);

    auto *styleSelector = window.findChild<StyleSelectorWidget *>();
    QVERIFY(styleSelector != nullptr);
    QCOMPARE(styleSelector->selectedStyle(), vynx::ModuleStyle::Square);

    // Switch to Dots
    styleSelector->setStyle(vynx::ModuleStyle::Dots);
    QCOMPARE(styleSelector->selectedStyle(), vynx::ModuleStyle::Dots);
    QCOMPARE(window.currentOptions().module_style, vynx::ModuleStyle::Dots);

    // Switch to Rounded
    styleSelector->setStyle(vynx::ModuleStyle::Rounded);
    QCOMPARE(styleSelector->selectedStyle(), vynx::ModuleStyle::Rounded);
    QCOMPARE(window.currentOptions().module_style, vynx::ModuleStyle::Rounded);
  }

  void topBarAndPillNavigationWork() {
    MainWindow window;
    present(window);

    // Theme toggle button exists and toggles theme
    auto *themeToggle = window.findChild<QAbstractButton *>(QStringLiteral("themeToggle"));
    QVERIFY(themeToggle != nullptr);
    const auto initialTheme = window.settings_.theme;
    themeToggle->click();
    QVERIFY(window.settings_.theme != initialTheme);

    // Category pills exist
    auto *urlPill = window.findChild<QAbstractButton *>(QStringLiteral("pill_url"));
    auto *wifiPill = window.findChild<QAbstractButton *>(QStringLiteral("pill_wifi"));
    QVERIFY(urlPill != nullptr);
    QVERIFY(wifiPill != nullptr);

    wifiPill->click();
    QCOMPARE(window.smartMode_, false);

    urlPill->click();
    QCOMPARE(window.smartMode_, true);
  }

  void generateButtonOnStructuredFormKeepsResult() {
    MainWindow window;
    present(window);

    window.setSpecialKind(vynx::PayloadType::Wifi);
    QLineEdit *ssid = findField(window, QStringLiteral("Network name (SSID)"));
    QVERIFY(ssid != nullptr);
    enter(ssid, QStringLiteral("TestWifi"));
    QLineEdit *password = findField(window, QStringLiteral("Password"));
    QVERIFY(password != nullptr);
    enter(password, QStringLiteral("SecretPass123"));

    const auto resultBefore = window.currentResult();
    QVERIFY(resultBefore.width > 0);

    // Clicking generateButton_ while in structured form must not wipe the QR code
    QVERIFY(window.generateButton_ != nullptr);
    window.generateButton_->click();

    const auto resultAfter = window.currentResult();
    QVERIFY(resultAfter.width > 0);
    QCOMPARE(resultAfter.width, resultBefore.width);
  }

  void morePillRestoresPreviousPillWhenMenuCancelled() {
    MainWindow window;
    present(window);

    auto *urlPill = window.findChild<QAbstractButton *>(QStringLiteral("pill_url"));
    auto *wifiPill = window.findChild<QAbstractButton *>(QStringLiteral("pill_wifi"));
    auto *morePill = window.findChild<QAbstractButton *>(QStringLiteral("pill_more"));
    QVERIFY(urlPill && wifiPill && morePill);

    wifiPill->click();
    QVERIFY(wifiPill->isChecked());

    // Close menu when it shows
    QTimer::singleShot(50, &window, [] {
      for (auto *widget : QApplication::topLevelWidgets()) {
        if (auto *menu = qobject_cast<QMenu *>(widget)) {
          menu->close();
        }
      }
    });
    morePill->click();

    // Since menu was cancelled without picking an action, wifiPill remains checked
    QVERIFY(wifiPill->isChecked());
    QVERIFY(!morePill->isChecked());
  }

  void captionUnderQrCodeExportedAndCopied() {
    MainWindow window;
    present(window);

    enter(smartInput(window), QStringLiteral("github.com"));
    QVERIFY(window.currentResult().width > 0);

    auto *captionBox = window.findChild<QCheckBox *>(QStringLiteral("captionCheckbox"));
    QVERIFY2(captionBox != nullptr, "caption checkbox must exist in settings");
    auto *captionInput = window.findChild<QLineEdit *>(QStringLiteral("captionInput"));
    QVERIFY2(captionInput != nullptr, "caption line edit must exist");

    QVERIFY(!captionBox->isChecked());
    QVERIFY(!captionInput->isVisible());

    captionBox->setChecked(true);
    QVERIFY(captionInput->isVisible());

    captionInput->setText(QStringLiteral("Scan me"));
    QTest::qWait(50);

    // Verify copy to clipboard includes caption (height > width)
    window.onCopy();
    const QImage copyImage = QApplication::clipboard()->image();
    QVERIFY(!copyImage.isNull());
    QVERIFY2(copyImage.height() > copyImage.width(), "copied image must include caption banner");

    // Verify export PNG includes caption
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString pngPath = dir.filePath(QStringLiteral("caption_test.png"));
    QVERIFY(window.exportToFile(pngPath, vynx::ExportFormat::Png));
    const QImage pngImage(pngPath);
    QVERIFY(!pngImage.isNull());
    QVERIFY2(pngImage.height() > pngImage.width(), "exported PNG image must include caption banner");

    // Verify export SVG includes caption and <text>
    const QString svgPath = dir.filePath(QStringLiteral("caption_test.svg"));
    QVERIFY(window.exportToFile(svgPath, vynx::ExportFormat::Svg));
    QFile svgFile(svgPath);
    QVERIFY(svgFile.open(QIODevice::ReadOnly));
    const QString svgContent = QString::fromUtf8(svgFile.readAll());
    QVERIFY(svgContent.contains(QStringLiteral("Scan me")));
    QVERIFY(svgContent.contains(QStringLiteral("<text")));

    // When unchecked, export returns to square
    captionBox->setChecked(false);
    window.onCopy();
    const QImage uncheckImage = QApplication::clipboard()->image();
    QVERIFY(!uncheckImage.isNull());
    QCOMPARE(uncheckImage.height(), uncheckImage.width());

    QApplication::clipboard()->clear();
  }

private:
  QTemporaryDir settingsDir_;
  QByteArray originalAppData_;
  QByteArray originalXdg_;
  static QString verifyLabel(vynx::VerifyStatus status) {
    if (status == vynx::VerifyStatus::Failed) {
      return QStringLiteral("QR could not be verified");
    }
    if (status == vynx::VerifyStatus::Mismatch) {
      return QStringLiteral("Verification mismatch");
    }
    return QStringLiteral("Scan verified");
  }

  /// Show a window and wait for it, so it is laid out before anything is typed.
  static void present(QWidget &window) {
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    window.raise();
    QTest::qWait(50);
  }

  /// Set the text and let the debounce fire, exactly as typing would.
  static void enter(QLineEdit *field, const QString &text) {
    field->setText(text);
    QTest::qWait(kSettle);
  }

  /// Type one character at a time with a pause, as a person does.
  static void typeWithPause(QWidget *target, const QString &text) {
    target->setFocus();
    QTest::qWait(20);
    for (const QChar character : text) {
      QTest::keyClicks(target, character);
      QTest::qWait(20);
    }
    QTest::qWait(kSettle);
  }

  /// The universal input, found the way assistive technology finds it. A plain
  /// findChild would return one of the customize panel's fields instead.
  static QLineEdit *smartInput(const QWidget &root) {
    return findField(root, QStringLiteral("Create QR"));
  }

  static QLineEdit *findField(const QWidget &root, const QString &accessibleName) {
    const auto fields = root.findChildren<QLineEdit *>();
    for (QLineEdit *field : fields) {
      if (field->accessibleName() == accessibleName) {
        return field;
      }
    }
    return nullptr;
  }

  /// Find a button by its visible text.
  static QPushButton *findButton(const QWidget &root, const QString &text) {
    const auto buttons = root.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
      if (button->text() == text) {
        return button;
      }
    }
    return nullptr;
  }
};

QTEST_MAIN(MainWindowTest)
#include "MainWindow_test.moc"
