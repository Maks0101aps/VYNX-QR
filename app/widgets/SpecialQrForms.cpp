#include "SpecialQrForms.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Bridge.h"
#include "Theme.h"

namespace {

/// A labelled row, so every control has a visible name rather than relying on
/// the placeholder alone.
QWidget *field(const QString &label, QWidget *control, const QString &hint = QString()) {
  auto *row = new QWidget;
  auto *layout = new QVBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  auto *title = new QLabel(label);
  title->setProperty("role", QStringLiteral("caption"));
  layout->addWidget(title);

  control->setAccessibleName(label);
  layout->addWidget(control);

  if (!hint.isEmpty()) {
    auto *note = new QLabel(hint);
    note->setProperty("role", QStringLiteral("caption"));
    note->setWordWrap(true);
    layout->addWidget(note);
  }
  return row;
}

} // namespace

SpecialQrForms::SpecialQrForms(QWidget *parent) : QWidget(parent) {
  stack_ = new QStackedWidget(this);

  buildWifiForm();
  buildContactForm();
  buildEmailForm();
  buildPhoneForm();
  buildSmsForm();
  buildGeoForm();

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);
  layout->addWidget(stack_);
  layout->addStretch(1);

  addBackButton(layout);
}

void SpecialQrForms::addBackButton(QVBoxLayout *layout) {
  auto *back = new QPushButton(QStringLiteral("← Back to smart input"), this);
  back->setProperty("flat", true);
  back->setCursor(Qt::PointingHandCursor);
  back->setAccessibleName(QStringLiteral("Back to smart input"));
  connect(back, &QPushButton::clicked, this, &SpecialQrForms::backRequested);

  layout->addWidget(back);
}

vynx::PayloadType SpecialQrForms::kind() const { return kind_; }

void SpecialQrForms::showKind(vynx::PayloadType kind) {
  kind_ = kind;
  int index = 0;
  switch (kind) {
  case vynx::PayloadType::Wifi: index = 0; break;
  case vynx::PayloadType::VCard: index = 1; break;
  case vynx::PayloadType::Email: index = 2; break;
  case vynx::PayloadType::Phone: index = 3; break;
  case vynx::PayloadType::Sms: index = 4; break;
  case vynx::PayloadType::Geo: index = 5; break;
  default:
    // An unrecognised kind has no form to show, so there is nothing to switch to.
    return;
  }
  stack_->setCurrentIndex(index);
  markComplete();
}

void SpecialQrForms::markComplete() { emit edited(); }

bool SpecialQrForms::isComplete() const {
  switch (kind_) {
  case vynx::PayloadType::Wifi:
    return !wifiSsid_->text().isEmpty();
  case vynx::PayloadType::Email:
    return !emailTo_->text().trimmed().isEmpty();
  case vynx::PayloadType::Phone:
    return !phoneNumber_->text().isEmpty();
  case vynx::PayloadType::Sms:
    return !smsNumber_->text().isEmpty();
  case vynx::PayloadType::VCard:
    return !vcardFirst_->text().trimmed().isEmpty() ||
           !vcardLast_->text().trimmed().isEmpty() ||
           !vcardOrganisation_->text().trimmed().isEmpty() ||
           !vcardPhone_->text().trimmed().isEmpty() ||
           !vcardEmail_->text().trimmed().isEmpty();
  case vynx::PayloadType::Geo:
    return true;
  default:
    return false;
  }
}

vynx::Payload SpecialQrForms::payload() const {
  vynx::Payload payload;
  payload.kind = kind_;

  switch (kind_) {
  case vynx::PayloadType::Wifi:
    payload.wifi.ssid = vynxq::toRust(wifiSsid_->text());
    // Kept exactly as typed: leading and trailing spaces are part of the name
    // and part of the password, and trimming them breaks the code.
    payload.wifi.password = vynxq::toRust(wifiPassword_->text());
    payload.wifi.security = wifiSecurity_->currentIndex() == 1   ? vynx::WifiSecurity::Wep
                           : wifiSecurity_->currentIndex() == 2 ? vynx::WifiSecurity::None
                                                                 : vynx::WifiSecurity::Wpa;
    payload.wifi.hidden = wifiHidden_->isChecked();
    break;
  case vynx::PayloadType::VCard:
    payload.vcard.first_name = vynxq::toRust(vcardFirst_->text());
    payload.vcard.last_name = vynxq::toRust(vcardLast_->text());
    payload.vcard.organization = vynxq::toRust(vcardOrganisation_->text());
    payload.vcard.job_title = vynxq::toRust(vcardJobTitle_->text());
    payload.vcard.phone = vynxq::toRust(vcardPhone_->text());
    payload.vcard.email = vynxq::toRust(vcardEmail_->text());
    payload.vcard.website = vynxq::toRust(vcardWebsite_->text());
    payload.vcard.address = vynxq::toRust(vcardAddress_->text());
    payload.vcard.note = vynxq::toRust(vcardNote_->toPlainText());
    break;
  case vynx::PayloadType::Email:
    payload.email.to = vynxq::toRust(emailTo_->text());
    payload.email.subject = vynxq::toRust(emailSubject_->text());
    payload.email.body = vynxq::toRust(emailBody_->toPlainText());
    break;
  case vynx::PayloadType::Phone:
    payload.phone = vynxq::toRust(phoneNumber_->text());
    break;
  case vynx::PayloadType::Sms:
    payload.sms.number = vynxq::toRust(smsNumber_->text());
    payload.sms.message = vynxq::toRust(smsMessage_->toPlainText());
    break;
  case vynx::PayloadType::Geo:
    payload.geo.latitude = geoLatitude_->value();
    payload.geo.longitude = geoLongitude_->value();
    payload.geo.label = vynxq::toRust(geoLabel_->text());
    break;
  default:
    break;
  }
  return payload;
}

void SpecialQrForms::buildWifiForm() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);

  wifiSsid_ = new QLineEdit;
  layout->addWidget(field(QStringLiteral("Network name (SSID)"), wifiSsid_,
                         QStringLiteral("Spaces are kept exactly as typed.")));

  auto *passwordRow = new QWidget;
  auto *passwordLayout = new QHBoxLayout(passwordRow);
  passwordLayout->setContentsMargins(0, 0, 0, 0);
  passwordLayout->setSpacing(Tokens::SpaceSmall);
  wifiPassword_ = new QLineEdit;
  wifiPassword_->setEchoMode(QLineEdit::Password);
  // The accessible name belongs to the field the user types into, not to the row
  // that holds it and the reveal button. A screen reader announcing the container
  // would leave the control itself unnamed.
  wifiPassword_->setAccessibleName(QStringLiteral("Password"));
  passwordLayout->addWidget(wifiPassword_, 1);
  wifiReveal_ = new QPushButton(QStringLiteral("Show"));
  wifiReveal_->setCheckable(true);
  wifiReveal_->setCursor(Qt::PointingHandCursor);
  wifiReveal_->setAccessibleName(QStringLiteral("Show or hide the password"));
  passwordLayout->addWidget(wifiReveal_);
  layout->addWidget(field(QStringLiteral("Password"), passwordRow,
                         QStringLiteral("Kept exactly as typed, spaces included.")));

  connect(wifiReveal_, &QPushButton::toggled, this, [this](bool visible) {
    wifiPassword_->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    wifiReveal_->setText(visible ? QStringLiteral("Hide") : QStringLiteral("Show"));
  });

  wifiSecurity_ = new QComboBox;
  wifiSecurity_->addItems({QStringLiteral("WPA / WPA2 / WPA3"), QStringLiteral("WEP"),
                           QStringLiteral("None (open)")});
  layout->addWidget(field(QStringLiteral("Security"), wifiSecurity_));

  wifiHidden_ = new QCheckBox(QStringLiteral("Hidden network"));
  wifiHidden_->setToolTip(QStringLiteral("The network does not broadcast its name."));
  layout->addWidget(wifiHidden_);

  connect(wifiSecurity_, &QComboBox::currentIndexChanged, this, [this](int index) {
    // An open network has no password, so the field is hidden rather than left
    // in a state that looks meaningful.
    const bool needed = index != 2;
    wifiPassword_->setEnabled(needed);
    if (!needed) {
      wifiPassword_->clear();
    }
    emit edited();
  });
  connect(wifiSsid_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);
  connect(wifiPassword_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);
  connect(wifiHidden_, &QCheckBox::toggled, this, &SpecialQrForms::markComplete);

  stack_->addWidget(page);
}

void SpecialQrForms::buildContactForm() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceSmall);

  auto addField = [this, layout](QLineEdit *box, const QString &label,
                                  const QString &hint = QString()) {
    connect(box, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);
    layout->addWidget(field(label, box, hint));
  };

  vcardFirst_ = new QLineEdit;
  addField(vcardFirst_, QStringLiteral("First name"));
  vcardLast_ = new QLineEdit;
  addField(vcardLast_, QStringLiteral("Last name"));
  vcardOrganisation_ = new QLineEdit;
  addField(vcardOrganisation_, QStringLiteral("Organisation"));
  vcardJobTitle_ = new QLineEdit;
  addField(vcardJobTitle_, QStringLiteral("Job title"));
  vcardPhone_ = new QLineEdit;
  addField(vcardPhone_, QStringLiteral("Phone"));
  vcardEmail_ = new QLineEdit;
  addField(vcardEmail_, QStringLiteral("Email"));
  vcardWebsite_ = new QLineEdit;
  addField(vcardWebsite_, QStringLiteral("Website"), QStringLiteral("A scheme is added for you."));
  vcardAddress_ = new QLineEdit;
  addField(vcardAddress_, QStringLiteral("Address"));

  auto *note = new QPlainTextEdit;
  note->setFixedHeight(64);
  vcardNote_ = note;
  layout->addWidget(field(QStringLiteral("Note"), note));
  connect(note, &QPlainTextEdit::textChanged, this, &SpecialQrForms::markComplete);

  stack_->addWidget(page);
}

void SpecialQrForms::buildEmailForm() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);

  emailTo_ = new QLineEdit;
  emailTo_->setPlaceholderText(QStringLiteral("name@example.com"));
  layout->addWidget(field(QStringLiteral("To"), emailTo_,
                         QStringLiteral("Several recipients can be separated by a comma.")));
  emailSubject_ = new QLineEdit;
  layout->addWidget(field(QStringLiteral("Subject"), emailSubject_));

  emailBody_ = new QPlainTextEdit;
  emailBody_->setAccessibleName(QStringLiteral("Body"));
  emailBody_->setMinimumHeight(96);
  layout->addWidget(field(QStringLiteral("Body"), emailBody_));

  connect(emailTo_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);
  connect(emailSubject_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);
  connect(emailBody_, &QPlainTextEdit::textChanged, this, &SpecialQrForms::markComplete);

  stack_->addWidget(page);
}

void SpecialQrForms::buildPhoneForm() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);

  phoneNumber_ = new QLineEdit;
  phoneNumber_->setPlaceholderText(QStringLiteral("+380 99 123 4567"));
  layout->addWidget(field(QStringLiteral("Phone number"), phoneNumber_,
                         QStringLiteral("Formatting characters are removed for you.")));
  connect(phoneNumber_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);

  stack_->addWidget(page);
}

void SpecialQrForms::buildSmsForm() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);

  smsNumber_ = new QLineEdit;
  smsNumber_->setPlaceholderText(QStringLiteral("+380 99 123 4567"));
  layout->addWidget(field(QStringLiteral("Number"), smsNumber_));

  smsMessage_ = new QPlainTextEdit;
  smsMessage_->setAccessibleName(QStringLiteral("Message"));
  smsMessage_->setMinimumHeight(96);
  layout->addWidget(field(QStringLiteral("Message"), smsMessage_));

  connect(smsNumber_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);
  connect(smsMessage_, &QPlainTextEdit::textChanged, this, &SpecialQrForms::markComplete);

  stack_->addWidget(page);
}

void SpecialQrForms::buildGeoForm() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(Tokens::SpaceMedium);

  geoLatitude_ = new QDoubleSpinBox;
  geoLatitude_->setRange(-90.0, 90.0);
  geoLatitude_->setDecimals(6);
  geoLatitude_->setSingleStep(0.0001);
  layout->addWidget(field(QStringLiteral("Latitude"), geoLatitude_,
                         QStringLiteral("Between -90 and 90.")));

  geoLongitude_ = new QDoubleSpinBox;
  geoLongitude_->setRange(-180.0, 180.0);
  geoLongitude_->setDecimals(6);
  geoLongitude_->setSingleStep(0.0001);
  layout->addWidget(field(QStringLiteral("Longitude"), geoLongitude_,
                         QStringLiteral("Between -180 and 180.")));

  geoLabel_ = new QLineEdit;
  geoLabel_->setPlaceholderText(QStringLiteral("Kyiv"));
  layout->addWidget(field(QStringLiteral("Label"), geoLabel_,
                         QStringLiteral("Shown by the maps app when the code is scanned.")));

  connect(geoLatitude_, &QDoubleSpinBox::valueChanged, this, &SpecialQrForms::markComplete);
  connect(geoLongitude_, &QDoubleSpinBox::valueChanged, this, &SpecialQrForms::markComplete);
  connect(geoLabel_, &QLineEdit::textChanged, this, &SpecialQrForms::markComplete);

  stack_->addWidget(page);
}