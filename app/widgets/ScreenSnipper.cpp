#include "ScreenSnipper.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include "Theme.h"
#include "i18n/I18n.h"
#include "platform/PlatformIntegration.h"
#include "vynx-qr-bridge/src/lib.rs.h"

ScreenSnipper::ScreenSnipper(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) {
  setAttribute(Qt::WA_DeleteOnClose, false);
  setCursor(Qt::CrossCursor);
}

void ScreenSnipper::startSnipping() {
  QScreen *screen = QGuiApplication::primaryScreen();
  if (!screen) {
    emit snippingCancelled();
    return;
  }

  // Grab the primary screen before showing the overlay
  screenshot_ = screen->grabWindow(0).toImage();
  setGeometry(screen->geometry());
  selecting_ = false;
  startPos_ = QPoint();
  currentPos_ = QPoint();

  showFullScreen();
  raise();
  activateWindow();
}

QRect ScreenSnipper::selectionRect() const {
  return QRect(startPos_, currentPos_).normalized();
}

void ScreenSnipper::paintEvent(QPaintEvent *) {
  QPainter painter(this);

  // Draw background screenshot
  if (!screenshot_.isNull()) {
    painter.drawImage(rect(), screenshot_);
  }

  // Draw dimmed semi-transparent overlay
  painter.fillRect(rect(), QColor(0, 0, 0, 140));

  if (selecting_) {
    const QRect sel = selectionRect();
    if (!sel.isEmpty() && !screenshot_.isNull()) {
      // Restore selected area to full brightness
      painter.drawImage(sel, screenshot_, sel);

      // Draw border around selection
      QPen pen(PlatformIntegration::systemAccentColor(), 2);
      painter.setPen(pen);
      painter.drawRect(sel);
    }
  }

  // Draw instructions at top center
  painter.setPen(Qt::white);
  QFont font = painter.font();
  font.setPixelSize(14);
  font.setBold(true);
  painter.setFont(font);

  const QString hint = I18n::tr("Select area with QR code · Esc to cancel");
  const QRect textRect(0, 30, width(), 40);
  painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, hint);
}

void ScreenSnipper::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    selecting_ = true;
    startPos_ = event->pos();
    currentPos_ = event->pos();
    update();
  }
}

void ScreenSnipper::mouseMoveEvent(QMouseEvent *event) {
  if (selecting_) {
    currentPos_ = event->pos();
    update();
  }
}

void ScreenSnipper::mouseReleaseEvent(QMouseEvent *event) {
  if (selecting_ && event->button() == Qt::LeftButton) {
    selecting_ = false;
    currentPos_ = event->pos();
    update();
    processSelection();
  }
}

void ScreenSnipper::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    emit snippingCancelled();
  }
}

void ScreenSnipper::processSelection() {
  const QRect sel = selectionRect();
  close();

  if (sel.width() < 10 || sel.height() < 10 || screenshot_.isNull()) {
    emit decodeFailed();
    return;
  }

  QImage cropped = screenshot_.copy(sel).convertToFormat(QImage::Format_RGBA8888);
  const uchar *bits = cropped.constBits();
  const qint64 byteCount = cropped.sizeInBytes();

  rust::Slice<const uint8_t> pixelSlice(bits, static_cast<size_t>(byteCount));

  try {
    rust::String result = vynx::decode_qr_rgba(pixelSlice, static_cast<uint32_t>(cropped.width()), static_cast<uint32_t>(cropped.height()));
    const QString decodedText = QString::fromUtf8(result.data(), static_cast<qsizetype>(result.size()));
    if (!decodedText.isEmpty()) {
      emit codeDecoded(decodedText);
      return;
    }
  } catch (...) {
    // Decoding failed
  }

  emit decodeFailed();
}
