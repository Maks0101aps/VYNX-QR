#pragma once

#include <QWidget>

#include "vynx-qr-bridge/src/lib.rs.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QVBoxLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QPlainTextEdit;
class QStackedWidget;

/// The structured forms, one per payload kind.
///
/// Every control edits a field of the bridge payload and nothing else: there is
/// no second encoder here, and no normalisation of the user's text. What the
/// user typed is what reaches Rust, which is why a Wi-Fi password keeps its
/// spaces.
class SpecialQrForms final : public QWidget {
  Q_OBJECT

public:
  explicit SpecialQrForms(QWidget *parent = nullptr);

  /// Which form is on screen.
  [[nodiscard]] vynx::PayloadType kind() const;

  /// Switch forms. The fields already filled in are not carried over: a network
  /// name left in a Wi-Fi form should not appear in a contact card.
  void showKind(vynx::PayloadType kind);

  /// The payload as the engine should encode it.
  [[nodiscard]] vynx::Payload payload() const;

  /// True when the current form has everything it needs.
  [[nodiscard]] bool isComplete() const;

signals:
  /// A field changed, so the code should be re-rendered.
  void edited();

  /// The user asked to go back to the universal input.
  void backRequested();

private:
  void addBackButton(QVBoxLayout *layout);

  void buildWifiForm();
  void buildContactForm();
  void buildEmailForm();
  void buildPhoneForm();
  void buildSmsForm();
  void buildGeoForm();

  void addBackButton();
  void markComplete();

  QStackedWidget *stack_ = nullptr;
  vynx::PayloadType kind_ = vynx::PayloadType::Wifi;

  // Wi-Fi
  QLineEdit *wifiSsid_ = nullptr;
  QLineEdit *wifiPassword_ = nullptr;
  QComboBox *wifiSecurity_ = nullptr;
  QCheckBox *wifiHidden_ = nullptr;
  QPushButton *wifiReveal_ = nullptr;

  // Contact
  QLineEdit *vcardFirst_ = nullptr;
  QLineEdit *vcardLast_ = nullptr;
  QLineEdit *vcardOrganisation_ = nullptr;
  QLineEdit *vcardJobTitle_ = nullptr;
  QLineEdit *vcardPhone_ = nullptr;
  QLineEdit *vcardEmail_ = nullptr;
  QLineEdit *vcardWebsite_ = nullptr;
  QLineEdit *vcardAddress_ = nullptr;
  QPlainTextEdit *vcardNote_ = nullptr;

  // Email
  QLineEdit *emailTo_ = nullptr;
  QLineEdit *emailSubject_ = nullptr;
  QPlainTextEdit *emailBody_ = nullptr;

  // Phone
  QLineEdit *phoneNumber_ = nullptr;

  // SMS
  QLineEdit *smsNumber_ = nullptr;
  QPlainTextEdit *smsMessage_ = nullptr;

  // Location
  QDoubleSpinBox *geoLatitude_ = nullptr;
  QDoubleSpinBox *geoLongitude_ = nullptr;
  QLineEdit *geoLabel_ = nullptr;
};