#include "QrPreview.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStyle>
#include <QPixmap>
#include <QVBoxLayout>

#include "Bridge.h"
#include "Theme.h"

namespace {

/// Payload kinds and what a scanner will do with them.
QString verificationText(vynx::VerifyStatus status) {
  if (status == vynx::VerifyStatus::Failed) {
    return QStringLiteral("QR could not be verified");
  }
  if (status == vynx::VerifyStatus::Mismatch) {
    return QStringLiteral("Verification mismatch");
  }
  return QStringLiteral("Scan verified");
}

QString errorCorrectionText(vynx::EcLevel level) {
  if (level == vynx::EcLevel::H) return QStringLiteral("High");
  if (level == vynx::EcLevel::Q) return QStringLiteral("Quartile");
  if (level == vynx::EcLevel::M) return QStringLiteral("Medium");
  return QStringLiteral("Low");
}

/// True for the formats the engine can decode.
bool isLogoMimeData(const QMimeData *mime) {
  if (!mime->hasUrls()) {
    return false;
  }
  const QList<QUrl> urls = mime->urls();
  for (const QUrl &url : urls) {
    if (url.isLocalFile()) {
      const QString suffix = url.fileName().section(QLatin1Char('.'), -1).toLower();
      if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") ||
          suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("webp")) {
        return true;
      }
    }
  }
  return false;
}

} // namespace

QrPreview::QrPreview(QWidget *parent) : QWidget(parent) {
  setAcceptDrops(true);
  layout_ = new QHBoxLayout(this);
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(0);

  auto *column = new QVBoxLayout;
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(Tokens::SpaceSmall);

  image_ = new QLabel(this);
  image_->setAlignment(Qt::AlignCenter);
  image_->setMinimumSize(Tokens::PreviewMaximum, Tokens::PreviewMaximum);
  image_->setAccessibleName(QStringLiteral("Generated QR code"));
  // A drop target has to advertise itself.
  image_->setAcceptDrops(false);
  column->addWidget(image_, 1);

  verification_ = new QLabel(this);
  verification_->setAlignment(Qt::AlignCenter);
  column->addWidget(verification_);

  details_ = new QLabel(this);
  details_->setAlignment(Qt::AlignCenter);
  details_->setProperty("role", QStringLiteral("caption"));
  details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  column->addWidget(details_);

  layout_->addLayout(column, 1);
  showEmpty();
}

void QrPreview::release() {
  // Assigning an empty pixmap frees the decoded image rather than leaving the
  // previous one alive behind the label.
  image_->setPixmap(QPixmap());
}

void QrPreview::showEmpty() {
  release();
  image_->setText(QStringLiteral("Paste or type anything"));
  verification_->hide();
  details_->hide();
}

void QrPreview::showError(const QString &message) {
  release();
  image_->setText(QStringLiteral("Nothing to show"));
  setVerificationLabel(message, false);
  details_->hide();
}

void QrPreview::setVerificationLabel(const QString &text, bool good) {
  verification_->setText(text);
  verification_->setProperty("role",
                             good ? QStringLiteral("verificationGood")
                                  : QStringLiteral("verificationBad"));
  // A property change only takes effect on a re-polish.
  verification_->style()->unpolish(verification_);
  verification_->style()->polish(verification_);
  verification_->show();
}

void QrPreview::showResult(const vynx::GenerateResult &result) {
  const QImage image =
      QImage::fromData(reinterpret_cast<const uchar *>(result.png.data()),
                       static_cast<int>(result.png.size()), "PNG");
  if (image.isNull()) {
    showError(vynxq::genericError());
    return;
  }

  const QPixmap scaled = QPixmap::fromImage(image.scaled(
      Tokens::PreviewMaximum, Tokens::PreviewMaximum, Qt::KeepAspectRatio,
      // Nearest neighbour: a QR code is a grid, and smoothing turns it to mush.
      Qt::FastTransformation));
  image_->setPixmap(scaled);
  image_->setText(QString());

  setVerificationLabel(verificationText(result.verification_status),
                       result.verification_status == vynx::VerifyStatus::Verified);

  QStringList warnings;
  for (const vynx::Warning &warning : result.warnings) {
    warnings << vynxq::toQString(warning.message);
  }

  QStringList details;
  details << QStringLiteral("Version %1 · %2 modules")
                 .arg(result.version)
                 .arg(result.modules);
  details << QStringLiteral("Error correction %1").arg(errorCorrectionText(result.ec_level));
  details << QStringLiteral("Contrast %1:1").arg(result.contrast, 0, 'f', 1);
  // The logo forces the level up, and saying otherwise would be a lie.
  if (result.ec_adjusted) {
    details << QStringLiteral("raised for the logo");
  }
  if (result.has_reduced_verification) {
    details << QStringLiteral("checked again at half size");
  }
  details << warnings;

  details_->setText(details.join(QStringLiteral(" · ")));
  details_->show();
}

void QrPreview::dragEnterEvent(QDragEnterEvent *event) {
  if (isLogoMimeData(event->mimeData())) {
    event->acceptProposedAction();
    return;
  }
  QWidget::dragEnterEvent(event);
}

void QrPreview::dropEvent(QDropEvent *event) {
  const QMimeData *mime = event->mimeData();
  if (isLogoMimeData(mime)) {
    const QList<QUrl> urls = mime->urls();
    for (const auto &url : urls) {
      QMimeData candidate;
      candidate.setUrls({url});
      if (isLogoMimeData(&candidate)) {
        emit logoDropped(url.toLocalFile());
        event->acceptProposedAction();
        return;
      }
    }
  }
  QWidget::dropEvent(event);
}
