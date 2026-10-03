#pragma once

#include <QColor>
#include <QString>

#include <rust/cxx.h>

#include "vynx-qr-bridge/src/lib.rs.h"

/// Conversions between the bridge's types and Qt's.
///
/// These live in one place so no widget has to remember that a bridge string is
/// UTF-8 and not something that can be indexed as bytes, or that an exception is
/// how a fallible call reports failure.
namespace vynxq {

/// `rust::String` to QString, without a lossy round trip through a C string.
[[nodiscard]] QString toQString(const rust::String &value);

/// QString to `rust::String`, taking ownership as the bridge expects.
[[nodiscard]] rust::String toRust(const QString &value);

/// The message a `rust::Error` carries, already safe to show.
[[nodiscard]] QString errorMessage(const rust::Error &error);

/// What a user is told when a failure carries no usable text.
[[nodiscard]] QString genericError();

/// Run a bridge call, turning a thrown `rust::Error` into a message.
template <typename Fn>
[[nodiscard]] auto guarded(Fn &&call) -> decltype(call()) {
  try {
    return call();
  } catch (const rust::Error &error) {
    return decltype(call())(errorMessage(error));
  }
}

/// A colour as `#RRGGBB`, which is the only form the engine accepts.
[[nodiscard]] QString toHex(const QColor &colour);

/// True when `value` is `#RRGGBB` or `#AARRGGBB` with hex digits only.
[[nodiscard]] bool isHexColour(const QString &value);

/// Parse `#RRGGBB`, ignoring a leading alpha pair and a missing hash.
[[nodiscard]] QColor parseHexColour(const QString &value);

} // namespace vynxq