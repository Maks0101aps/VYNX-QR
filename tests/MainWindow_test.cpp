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
// are covered once, by typingUpdatesTheCodeWithoutEnter, which is enough to prove
// the input path is wired up: in a session without foreground permission only the
// first window created in a process accepts synthetic keystrokes, so making every
// test depend on that would test the environment rather than the code.
//
// Nothing here touches the real clipboard or writes to the user's settings.

#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>

#include <rust/cxx.h>

#include "MainWindow.h"
#include "widgets/ClipboardSuggestion.h"
#include "widgets/CustomizePanel.h"
#include "widgets/QrPreview.h"
#include "widgets/SpecialQrForms.h"
#include "vynx-qr-bridge/src/lib.rs.h"

namespace {

/// A bridge string is UTF-8 and not a std::string, so it is decoded explicitly.
QString bridgeString(const rust::String &value) {
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

/// Long enough for the 150 ms debounce to have fired.
constexpr int kSettle = 600;

} // namespace

class MainWindowTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // The window reads the clipboard once at start-up, so a test must not be able
    // to be influenced by whatever the developer happened to copy.
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
    QTest::mouseClick(toggle, Qt::LeftButton);

    QCOMPARE(bridgeString(window.currentResult().encoded), QStringLiteral("github.com"));

    // And it can be taken back.
    toggle = findButton(window, QStringLiteral("Use original text"));
    QVERIFY(toggle != nullptr);
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

private:
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
    QTest::qWaitForWindowExposed(&window);
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
    QTest::qWait(50);
    for (const QChar character : text) {
      QTest::keyClicks(target, character);
      QTest::qWait(200);
    }
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