#include "Bridge.h"

#include <QColor>
#include <QRegularExpression>

namespace vynxq {
namespace {

const QString kGenericError = QStringLiteral("The QR engine reported a problem.");

/// Accepts the forms a user actually types and the form the engine produces, and
/// rejects everything else rather than guessing a colour.
const QRegularExpression &hexPattern() {
  static const QRegularExpression pattern(
      QStringLiteral(R"(^#?(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$)"));
  return pattern;
}

} // namespace

QString toQString(const rust::String &value) {
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

rust::String toRust(const QString &value) {
  return rust::String(value.toUtf8().toStdString());
}

QString errorMessage(const rust::Error &error) {
  const QString message = QString::fromUtf8(error.what());
  return message.isEmpty() ? genericError() : message;
}

QString genericError() { return kGenericError; }

QString toHex(const QColor &colour) {
  return colour.name(QColor::HexRgb).toUpper();
}

bool isHexColour(const QString &value) {
  return hexPattern().match(value.trimmed()).hasMatch();
}

QColor parseHexColour(const QString &value) {
  QString text = value.trimmed();
  if (!text.startsWith(QLatin1Char('#'))) {
    text.prepend(QLatin1Char('#'));
  }
  if (text.length() == 4) {
    // Shorthand #abc means #aabbcc.
    QString expanded;
    for (const QChar character : text) {
      if (character == QLatin1Char('#')) {
        continue;
      }
      expanded.append(character).append(character);
    }
    text = expanded;
  }
  // A QR code has no alpha, so a leading pair is dropped rather than applied.
  if (text.length() == 9) {
    text = text.left(1) + text.mid(3);
  }
  return QColor(text);
}

} // namespace vynxq