#include "app_banner.h"
#include "piece_renderer.h"
#include "ui_theme.h"
#include <QColor>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QResizeEvent>

namespace {

constexpr qreal kWordmarkPadX = 16.0;
constexpr qreal kWordmarkPadY = 5.0;
constexpr qreal kBezelRadius = 10.0;

QRectF wordmarkDest(const QImage &wordmark, int bannerW, int bannerH)
{
    if (wordmark.isNull() || wordmark.width() <= 0 || wordmark.height() <= 0) {
        return QRectF();
    }
    const qreal maxW = qMax(1.0, bannerW - kWordmarkPadX * 2.0);
    const qreal maxH = qMax(1.0, bannerH - kWordmarkPadY * 2.0);
    const qreal scale = qMin(maxW / wordmark.width(), maxH / wordmark.height());
    const qreal destW = wordmark.width() * scale;
    const qreal destH = wordmark.height() * scale;
    return QRectF(kWordmarkPadX, (bannerH - destH) * 0.5, destW, destH);
}

int boardStripWidth(int bannerW, int bannerH, const QImage &wordmark)
{
    bannerW = qMax(1, bannerW);
    const QRectF logo = wordmarkDest(wordmark, bannerW, bannerH);
    if (logo.isEmpty()) {
        return qMax(1, bannerW / 2);
    }
    const int overlap = int(qRound(logo.width() * 0.32));
    return qMax(1, bannerW - int(qRound(logo.right())) + overlap);
}

QImage fadedBoardStrip(const QImage &source, int width, int height, int fadePx,
                       qreal dpr)
{
    width = qMax(1, width);
    height = qMax(1, height);
    dpr = qMax(1.0, dpr);
    fadePx = qMax(1, fadePx);
    const int physW = qMax(1, int(qRound(width * dpr)));
    const int physH = qMax(1, int(qRound(height * dpr)));
    const int fadePhys = qMax(1, qMin(physW, int(qRound(fadePx * dpr))));
    QImage image(physW, physH, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return image;
    }

    const qreal scale = qMax(qreal(physW) / source.width(),
                             qreal(physH) / source.height());
    const qreal dw = source.width() * scale;
    const qreal dh = source.height() * scale;
    const qreal dx = physW - dw;
    const qreal dy = (physH - dh) / 2.0;

    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF(dx, dy, dw, dh), source, QRectF(source.rect()));
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient fade(0, 0, fadePhys, 0);
    fade.setColorAt(0.00, QColor(0, 0, 0, 0));
    fade.setColorAt(0.20, QColor(0, 0, 0, 50));
    fade.setColorAt(0.50, QColor(0, 0, 0, 170));
    fade.setColorAt(0.78, QColor(0, 0, 0, 240));
    fade.setColorAt(1.00, QColor(0, 0, 0, 255));
    painter.fillRect(0, 0, fadePhys, physH, fade);
    if (fadePhys < physW) {
        painter.fillRect(fadePhys, 0, physW - fadePhys, physH,
                         QColor(0, 0, 0, 255));
    }
    painter.end();
    return image;
}

}  // namespace

AppBanner::AppBanner(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(76);
    setAccessibleName(QStringLiteral("About Shatranj"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(QStringLiteral("About Shatranj"));
}

void AppBanner::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter ||
         event->key() == Qt::Key_Space) &&
        clicked) {
        clicked();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AppBanner::mousePressEvent(QMouseEvent *event)
{ pressed_ = event->button() == Qt::LeftButton; }

void AppBanner::mouseReleaseEvent(QMouseEvent *event)
{
    const bool activate = pressed_ && event->button() == Qt::LeftButton && rect().contains(event->pos());
    pressed_ = false;
    if (activate && clicked) clicked();
}

void AppBanner::resizeEvent(QResizeEvent *event)
{
    boardStrip_ = QImage();
    QWidget::resizeEvent(event);
}

void AppBanner::ensureImages()
{
    if (imagesLoaded_) {
        return;
    }
    imagesLoaded_ = true;
    wordmarkImage_ = QImage(PieceRenderer::assetPath(
        QStringLiteral("assets/pc-client/about/banner-wordmark.png")));
    boardImage_ = QImage(PieceRenderer::assetPath(
        QStringLiteral("assets/pc-client/about/banner-board.png")));
    if (boardImage_.isNull()) {
        mosaicImage_ = QImage(PieceRenderer::assetPath(
            QStringLiteral("assets/pc-client/about/banner-mosaic.png")));
        if (mosaicImage_.isNull()) {
            mosaicImage_ = QImage(620, 70, QImage::Format_RGB32);
            mosaicImage_.fill(QColor(22, 22, 30));
        }
    }
}

void AppBanner::ensureBoardStrip()
{
    ensureImages();
    if (boardImage_.isNull()) {
        boardStrip_ = QImage();
        return;
    }
    const int stripW = boardStripWidth(width(), height(), wordmarkImage_);
    const int fadePx = qMax(48, int(stripW * 0.46));
    const qreal dpr = qMax(1.0, devicePixelRatioF());
    const int physW = qMax(1, int(qRound(stripW * dpr)));
    const int physH = qMax(1, int(qRound(height() * dpr)));
    if (!boardStrip_.isNull() && boardStrip_.width() == physW &&
        boardStrip_.height() == physH) {
        return;
    }
    boardStrip_ = fadedBoardStrip(boardImage_, stripW, height(), fadePx, dpr);
}

void AppBanner::paintEvent(QPaintEvent *)
{
    ensureImages();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(bounds, kBezelRadius, kBezelRadius);
    painter.setClipPath(clip);
    painter.fillRect(rect(), QColor(22, 22, 30));

    ensureBoardStrip();
    const int stripW = boardStrip_.isNull()
                           ? 0
                           : boardStripWidth(width(), height(), wordmarkImage_);
    if (stripW > 0) {
        painter.drawImage(QRect(width() - stripW, 0, stripW, height()),
                          boardStrip_, boardStrip_.rect());
    } else {
        painter.drawImage(rect(), mosaicImage_);
    }

    const QRectF logo = wordmarkDest(wordmarkImage_, width(), height());
    if (!logo.isEmpty()) {
        painter.drawImage(logo, wordmarkImage_);
    }

    painter.setClipping(false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(SHZ_BORDER_SOFT), 1));
    painter.drawRoundedRect(bounds, kBezelRadius, kBezelRadius);
}
