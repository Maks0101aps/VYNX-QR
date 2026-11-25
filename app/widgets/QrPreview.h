#pragma once

#include <QLabel>
#include <QWidget>
#include <QColor>

#include "vynx-qr-bridge/src/lib.rs.h"

class QHBoxLayout;

/// The QR code itself, with its verification state and its technical details.
///
/// It draws nothing itself: the engine hands over finished PNG bytes and this
/// widget only decides how to present them. Scaling is nearest neighbour on
/// purpose, because smoothing a QR code blurs the module edges that make it
/// readable.
class QrPreview final : public QWidget {
  Q_OBJECT

public:
  explicit QrPreview(QWidget *parent = nullptr);

  /// Show the result of a render.
  void showResult(const vynx::GenerateResult &result);

  /// Show the empty state, before anything has been typed.
  void showEmpty();

  /// Show a failure instead of a code.
  void showError(const QString &message);

  /// Drop the current image, releasing the buffer.
  ///
  /// Called when the preview is hidden and before a new render, because a
  /// 1024 pixel RGBA buffer is four megabytes and there is no reason to keep one
  /// around while the user is still typing.
  void release();

  /// Set the caption text and foreground color shown under the QR code.
  void setCaption(const QString &caption, const QColor &color = QColor());

  /// Get current caption text.
  [[nodiscard]] QString caption() const { return captionText_; }

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

signals:
  /// The user dropped an image onto the code.
  void logoDropped(const QString &path);

private:
  void setVerificationLabel(const QString &text, bool good);
  void updateCaptionDisplay();

  QLabel *image_ = nullptr;
  QLabel *captionLabel_ = nullptr;
  QLabel *verification_ = nullptr;
  QLabel *details_ = nullptr;
  QHBoxLayout *layout_ = nullptr;

  QString captionText_;
  QColor captionColor_ = QColor("#000000");
};