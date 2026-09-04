#include "piece_renderer.h"

#include "ui_theme.h"

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSvgRenderer>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace PieceRenderer {

struct SquareIconKey {
    quint16 squareSize = 0;
    quint16 pieceIconSize = 0;
    qint8 row = 0;
    qint8 col = 0;
    char piece = 0;
    quint8 flags = 0;

    bool operator==(const SquareIconKey &other) const
    {
        return squareSize == other.squareSize &&
               pieceIconSize == other.pieceIconSize &&
               row == other.row &&
               col == other.col &&
               piece == other.piece &&
               flags == other.flags;
    }
};

static_assert(sizeof(SquareIconKey) == 8,
              "SquareIconKey must stay packed for qHashBits");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline size_t qHash(const SquareIconKey &key, size_t seed = 0) noexcept
{
    return qHashBits(&key, sizeof(key), seed);
}
#else
inline uint qHash(const SquareIconKey &key, uint seed = 0)
{
    return qHashBits(&key, sizeof(key), seed);
}
#endif

static QString g_pieceSet;
static QString g_boardTexture;
static QHash<QString, QIcon> s_iconCache;
static QHash<SquareIconKey, QIcon> s_squareIconCache;
static QString s_boardCacheName;
static int s_boardCacheSquareSize = 0;
static QImage s_boardCache;
static QStringList s_pcClientRoots;
static bool s_pcClientRootsResolved = false;
static QColor s_wellColor;
static QString s_wellTexture;
static int s_wellSquareSize = -1;

// --- Piece set discovery ---

static bool pieceSetHasAllPieces(const QDir &dir)
{
    static const char *required[] = {
        "wP.svg", "wN.svg", "wB.svg", "wR.svg", "wQ.svg", "wK.svg",
        "bP.svg", "bN.svg", "bB.svg", "bR.svg", "bQ.svg", "bK.svg",
    };
    for (const char *name : required) {
        if (!QFileInfo::exists(dir.filePath(QString::fromLatin1(name)))) {
            return false;
        }
    }
    return true;
}

QStringList pieceSets()
{
    QStringList sets;
    const QDir d(assetPath(QStringLiteral("assets/pc-client/piece_sets")));
    if (!d.exists()) return sets;
    const auto entries = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &e : entries) {
        if (pieceSetHasAllPieces(QDir(d.filePath(e)))) {
            sets.append(e);
        }
    }
    return sets;
}

void setPieceSet(const QString &name) { g_pieceSet = name; clearIconCache(); }
QString pieceSet() { return g_pieceSet; }

// --- Board texture discovery ---

QStringList boardTextures()
{
    QStringList boards;
    const QDir d(assetPath(QStringLiteral("assets/pc-client/boards")));
    if (!d.exists()) return boards;
    const auto entries = d.entryList({"*.png", "*.jpg", "*.jpeg"}, QDir::Files, QDir::Name);
    for (const QString &e : entries) boards.append(e);
    return boards;
}

void setBoardTexture(const QString &name)
{
    g_boardTexture = name;
    s_squareIconCache.clear();
    s_boardCache = QImage();
    s_boardCacheName.clear();
    s_boardCacheSquareSize = 0;
    s_wellColor = QColor();
    s_wellTexture.clear();
    s_wellSquareSize = -1;
}
QString boardTexture() { return g_boardTexture; }

static const char *cornerRadiusProperty(SquareCorner corner)
{
    switch (corner) {
    case CornerTopLeft:     return "border-top-left-radius";
    case CornerTopRight:    return "border-top-right-radius";
    case CornerBottomLeft:  return "border-bottom-left-radius";
    case CornerBottomRight: return "border-bottom-right-radius";
    case CornerNone:        break;
    }
    return nullptr;
}

QString boardSquareStyle(int row, int col, int squareSize,
                         const QString &background, const QString &foreground,
                         const QString &border, SquareCorner corner)
{
    const bool light = ((row + col) % 2) == 0;
    const QString defaultBg = light ? QStringLiteral(SHZ_SQ_LIGHT)
                                    : QStringLiteral(SHZ_SQ_DARK);
    const bool textured = background == defaultBg &&
                          !boardTextureImage(squareSize).isNull();
    const QString bg = textured ? QStringLiteral("transparent") : background;
    QString roundedVertex;
    if (const char *property = cornerRadiusProperty(corner)) {
        roundedVertex = QString(" %1:%2px;").arg(QLatin1String(property))
                                            .arg(SHZ_R_SQ);
    }
    return QString("QPushButton { background:%1; color:%2; border:%3;"
                   " border-radius:0;%4 font-weight:700; font-size:24px;"
                   " padding:0; margin:0; min-height:0; }")
        .arg(bg, foreground, border, roundedVertex);
}

// --- Procedural drawing (fallback) ---

QPainterPath roundedRectPath(double x, double y, double w, double h, double r)
{
    QPainterPath path;
    path.addRoundedRect(QRectF(x, y, w, h), r, r);
    return path;
}

void drawBase(QPainter &p, const QBrush &fill, const QPen &stroke)
{
    p.setPen(stroke);
    p.setBrush(fill);
    p.drawPath(roundedRectPath(13, 38, 26, 6, 2));
    p.drawPath(roundedRectPath(9, 44, 34, 5, 2));
}

void drawPawn(QPainter &p, const QBrush &fill, const QPen &stroke)
{
    p.setPen(stroke); p.setBrush(fill);
    p.drawEllipse(QPointF(26, 14), 8, 8);
    p.drawPath(roundedRectPath(20, 22, 12, 19, 5));
    drawBase(p, fill, stroke);
}

void drawRook(QPainter &p, const QBrush &fill, const QPen &stroke)
{
    QPainterPath crown;
    crown.moveTo(14, 10); crown.lineTo(19, 10); crown.lineTo(19, 15);
    crown.lineTo(24, 15); crown.lineTo(24, 10); crown.lineTo(29, 10);
    crown.lineTo(29, 15); crown.lineTo(34, 15); crown.lineTo(34, 10);
    crown.lineTo(39, 10); crown.lineTo(39, 20); crown.lineTo(14, 20);
    crown.closeSubpath();
    p.setPen(stroke); p.setBrush(fill);
    p.drawPath(crown);
    p.drawPath(roundedRectPath(17, 20, 18, 21, 2));
    drawBase(p, fill, stroke);
}

void drawKnight(QPainter &p, const QBrush &fill, const QPen &stroke)
{
    QPainterPath head;
    head.moveTo(17, 40);
    head.cubicTo(18, 31, 18, 23, 23, 16);
    head.cubicTo(27, 10, 34, 9, 39, 13);
    head.cubicTo(36, 16, 37, 19, 42, 23);
    head.lineTo(35, 26);
    head.cubicTo(38, 32, 35, 38, 30, 40);
    head.closeSubpath();
    p.setPen(stroke); p.setBrush(fill);
    p.drawPath(head);
    p.drawLine(28, 18, 24, 24);
    p.drawEllipse(QPointF(33, 16), 1.5, 1.5);
    drawBase(p, fill, stroke);
}

void drawBishop(QPainter &p, const QBrush &fill, const QPen &stroke, const QPen &detail)
{
    p.setPen(stroke); p.setBrush(fill);
    p.drawEllipse(QPointF(26, 13), 6, 6);
    p.drawPath(roundedRectPath(18, 19, 16, 22, 8));
    p.setPen(detail);
    p.drawLine(29, 21, 22, 33);
    drawBase(p, fill, stroke);
}

void drawQueen(QPainter &p, const QBrush &fill, const QPen &stroke)
{
    QPainterPath body;
    body.moveTo(15, 37); body.lineTo(20, 17); body.lineTo(26, 31);
    body.lineTo(32, 17); body.lineTo(37, 37); body.closeSubpath();
    p.setPen(stroke); p.setBrush(fill);
    p.drawEllipse(QPointF(20, 13), 4, 4);
    p.drawEllipse(QPointF(26, 10), 4, 4);
    p.drawEllipse(QPointF(32, 13), 4, 4);
    p.drawPath(body);
    drawBase(p, fill, stroke);
}

void drawKing(QPainter &p, const QBrush &fill, const QPen &stroke)
{
    p.setPen(stroke); p.setBrush(fill);
    p.drawLine(26, 7, 26, 18);
    p.drawLine(21, 12, 31, 12);
    p.drawEllipse(QPointF(26, 22), 8, 8);
    p.drawPath(roundedRectPath(18, 27, 16, 14, 5));
    drawBase(p, fill, stroke);
}

// --- Asset path resolution ---

QString pieceAssetName(char piece)
{
    const bool white = piece >= 'A' && piece <= 'Z';
    const char lower = white ? static_cast<char>(piece + 32) : piece;
    char type = '\0';
    switch (lower) {
    case 'p': type = 'P'; break;
    case 'r': type = 'R'; break;
    case 'n': type = 'N'; break;
    case 'b': type = 'B'; break;
    case 'q': type = 'Q'; break;
    case 'k': type = 'K'; break;
    default:  return QString();
    }
    return QString("%1%2").arg(white ? QLatin1Char('w') : QLatin1Char('b'))
                          .arg(QLatin1Char(type));
}

static const QStringList &pcClientRoots()
{
    if (s_pcClientRootsResolved) {
        return s_pcClientRoots;
    }
    s_pcClientRootsResolved = true;
    const QString relative = QStringLiteral("assets/pc-client");
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bases = {
        QDir::cleanPath(appDir + QLatin1Char('/') + relative),
        QDir::cleanPath(appDir + QStringLiteral("/../Resources/") + relative),
        QDir::cleanPath(appDir + QStringLiteral("/../../") + relative),
        QDir::cleanPath(QDir::currentPath() + QLatin1Char('/') + relative),
    };
    for (const QString &p : bases) {
        if (QFileInfo::exists(p)) {
            s_pcClientRoots.append(p);
        }
    }
    return s_pcClientRoots;
}

QString assetPath(const QString &relativePath)
{
    const QString prefix = QStringLiteral("assets/pc-client");
    if (relativePath == prefix || relativePath.startsWith(prefix + QLatin1Char('/'))) {
        const QStringList &roots = pcClientRoots();
        if (roots.isEmpty()) {
            return QString();
        }
        if (relativePath == prefix) {
            return roots.constFirst();
        }
        const QString rest = relativePath.mid(prefix.size() + 1);
        for (const QString &root : roots) {
            const QString path = QDir(root).filePath(rest);
            if (QFileInfo::exists(path)) {
                return QDir::cleanPath(path);
            }
        }
        return QString();
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bases = {
        QDir::cleanPath(appDir + QLatin1Char('/') + relativePath),
        QDir::cleanPath(appDir + QStringLiteral("/../Resources/") + relativePath),
        QDir::cleanPath(appDir + QStringLiteral("/../../") + relativePath),
        QDir::cleanPath(QDir::currentPath() + QLatin1Char('/') + relativePath),
    };
    for (const QString &p : bases) {
        if (QFileInfo::exists(p)) {
            return p;
        }
    }
    return QString();
}

QString pieceAssetPath(char piece)
{
    const QString name = pieceAssetName(piece) + ".png";
    return assetPath("assets/pc-client/pieces/" + name);
}

QString pieceSvgPath(char piece)
{
    if (g_pieceSet.isEmpty()) return QString();
    const QString name = pieceAssetName(piece) + ".svg";
    return assetPath("assets/pc-client/piece_sets/" + g_pieceSet + "/" + name);
}

// --- Icon rendering ---

static bool s_prewarmed = false;

QIcon pieceIcon(char piece)
{
    if (piece == '.') return QIcon();

    const QString cacheKey = g_pieceSet.isEmpty()
        ? QString(QChar(piece))
        : g_pieceSet + QChar(piece);
    const auto cached = s_iconCache.constFind(cacheKey);
    if (cached != s_iconCache.constEnd())
        return cached.value();

    // Try SVG first
    if (!g_pieceSet.isEmpty()) {
        const QString svgPath = pieceSvgPath(piece);
        if (!svgPath.isEmpty()) {
            QPixmap pixmap(52, 52);
            pixmap.fill(Qt::transparent);
            QSvgRenderer renderer(svgPath);
            if (renderer.isValid()) {
                QPainter p(&pixmap);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                renderer.render(&p, QRectF(0, 0, 52, 52));
                p.end();
                QIcon icon(pixmap);
                s_iconCache.insert(cacheKey, icon);
                return icon;
            }
        }
    }

    // Try PNG fallback
    const QString pngPath = pieceAssetPath(piece);
    if (!pngPath.isEmpty()) {
        QIcon icon(pngPath);
        s_iconCache.insert(cacheKey, icon);
        return icon;
    }

    // Procedural fallback
    const bool white = piece >= 'A' && piece <= 'Z';
    QPixmap pixmap(52, 52);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    const QColor fillColor = white ? QColor("#f7f0de") : QColor("#1d241f");
    const QColor strokeColor = white ? QColor("#222820") : QColor("#f7f0de");
    const QColor detailColor = white ? QColor("#5b604f") : QColor("#d5c7a8");
    const QBrush fill(fillColor);
    const QPen stroke(strokeColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen detail(detailColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    switch (piece >= 'A' && piece <= 'Z' ? static_cast<char>(piece + 32) : piece) {
    case 'p': drawPawn(p, fill, stroke); break;
    case 'r': drawRook(p, fill, stroke); break;
    case 'n': drawKnight(p, fill, stroke); break;
    case 'b': drawBishop(p, fill, stroke, detail); break;
    case 'q': drawQueen(p, fill, stroke); break;
    case 'k': drawKing(p, fill, stroke); break;
    default: return QIcon();
    }
    p.end();
    QIcon icon(pixmap);
    s_iconCache.insert(cacheKey, icon);
    return icon;
}

void prewarmPieceIcons()
{
    if (s_prewarmed || g_pieceSet.isEmpty()) {
        return;
    }
    s_prewarmed = true;
    for (const char piece : QByteArray("PNBRQKpnbrqk")) {
        (void)pieceIcon(piece);
    }
}

void clearIconCache()
{
    s_iconCache.clear();
    s_squareIconCache.clear();
    s_prewarmed = false;
    prewarmPieceIcons();
}

// --- Board texture ---

QImage boardTextureImage(int squareSize)
{
    if (g_boardTexture.isEmpty()) return QImage();
    if (s_boardCacheName == g_boardTexture &&
        s_boardCacheSquareSize == squareSize &&
        !s_boardCache.isNull()) {
        return s_boardCache;
    }
    const QString path = assetPath("assets/pc-client/boards/" + g_boardTexture);
    if (path.isEmpty()) return QImage();
    QImage img(path);
    if (img.isNull()) return QImage();
    if (img.depth() == 1) {
        img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    const int totalPx = squareSize * 8;
    s_boardCacheName = g_boardTexture;
    s_boardCacheSquareSize = squareSize;
    s_boardCache = img.scaled(totalPx, totalPx, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation);
    return s_boardCache;
}

QColor boardWellColor(int squareSize)
{
    if (s_wellTexture == g_boardTexture && s_wellSquareSize == squareSize &&
        s_wellColor.isValid()) {
        return s_wellColor;
    }
    const QImage board = boardTextureImage(squareSize);
    if (board.isNull() || squareSize <= 0) {
        s_wellColor = QColor(SHZ_BOARD_WELL);
    } else {
        // Average of one central dark square (edges and vignettes lie), darkened
        // so the frame still reads as a well and the coordinates keep contrast.
        const QImage sample = board.copy(4 * squareSize, 3 * squareSize,
                                         squareSize, squareSize)
                                   .scaled(1, 1, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
        s_wellColor = sample.pixelColor(0, 0).darker(190);
    }
    s_wellTexture = g_boardTexture;
    s_wellSquareSize = squareSize;
    return s_wellColor;
}

// Textured squares carry the board image inside the icon, where a style sheet
// radius cannot reach it, so the blunt vertex is painted into the pixmap.
static QPixmap bluntSquareVertex(const QPixmap &square, SquareCorner corner,
                                 int squareSize)
{
    if (corner == CornerNone) {
        return square;
    }
    const qreal r = SHZ_R_SQ;
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, squareSize, squareSize), r, r);
    QPainterPath squaredOff;
    if (corner != CornerTopLeft)     squaredOff.addRect(0, 0, r, r);
    if (corner != CornerTopRight)    squaredOff.addRect(squareSize - r, 0, r, r);
    if (corner != CornerBottomLeft)  squaredOff.addRect(0, squareSize - r, r, r);
    if (corner != CornerBottomRight) squaredOff.addRect(squareSize - r, squareSize - r, r, r);
    path = path.united(squaredOff);

    QPixmap out(square.size());
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(square));
    painter.drawPath(path);
    return out;
}

QIcon boardSquareIcon(char piece, int row, int col, int squareSize,
                      int pieceIconSize, bool pieceVisible, bool legalHint,
                      bool targetHighlight, SquareCorner corner)
{
    SquareIconKey cacheKey;
    cacheKey.squareSize = static_cast<quint16>(qBound(0, squareSize, 0xffff));
    cacheKey.pieceIconSize = static_cast<quint16>(qBound(0, pieceIconSize, 0xffff));
    cacheKey.row = static_cast<qint8>(row);
    cacheKey.col = static_cast<qint8>(col);
    cacheKey.piece = piece;
    cacheKey.flags = static_cast<quint8>(
        (pieceVisible ? 1u : 0u) |
        (legalHint ? 2u : 0u) |
        (targetHighlight ? 4u : 0u) |
        (static_cast<unsigned>(corner) << 3));
    const auto cached = s_squareIconCache.constFind(cacheKey);
    if (cached != s_squareIconCache.constEnd()) {
        return cached.value();
    }

    const QImage board = boardTextureImage(squareSize);
    if (board.isNull()) {
        return pieceVisible ? pieceIcon(piece) : QIcon();
    }

    QPixmap pixmap = QPixmap::fromImage(
        board.copy(col * squareSize, row * squareSize, squareSize, squareSize));
    if ((pieceVisible && piece != '.') || legalHint || targetHighlight) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (targetHighlight) {
            painter.setPen(QPen(QColor(47, 95, 159), 3));
            painter.setBrush(QColor(159, 184, 217, 96));
            painter.drawRect(QRect(1, 1, squareSize - 2, squareSize - 2));
        }
        if (pieceVisible && piece != '.') {
            const QPixmap piecePixmap = pieceIcon(piece).pixmap(pieceIconSize, pieceIconSize);
            const int offset = (squareSize - pieceIconSize) / 2;
            painter.drawPixmap(offset, offset, piecePixmap);
        } else if (legalHint) {
            const int r = squareSize / 10 < 4 ? 4 : squareSize / 10;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 96));
            painter.drawEllipse(QPointF(squareSize / 2.0, squareSize / 2.0), r, r);
        }
    }
    pixmap = bluntSquareVertex(pixmap, corner, squareSize);
    QIcon icon(pixmap);
    s_squareIconCache.insert(cacheKey, icon);
    return icon;
}

// --- App icon ---

QIcon makeShatranjIcon()
{
    static const int kSizes[] = {16, 32, 48, 256};
    QIcon icon;
    const QImage master(assetPath(QStringLiteral("assets/pc-client/about/app-icon.png")));
    if (!master.isNull()) {
        for (int size : kSizes) {
            icon.addPixmap(QPixmap::fromImage(
                master.scaled(size, size, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation)));
        }
        return icon;
    }

    for (int size : kSizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const qreal inset = size >= 32 ? 0.5 : 0.0;
        const qreal radius = size * 0.22;
        const QPainterPath badge = roundedRectPath(
            inset, inset, size - inset * 2.0, size - inset * 2.0, radius);
        painter.fillPath(badge, QColor(QStringLiteral(SHZ_INK)));
        painter.setPen(QPen(QColor(196, 165, 116), size >= 32 ? 1.5 : 1.0));
        painter.drawPath(badge);
        const qreal scale = size / 56.0;
        painter.translate((size - 52.0 * scale) * 0.5, size * 0.06);
        painter.scale(scale, scale);
        drawKnight(painter, QBrush(QColor(236, 220, 186)),
                   QPen(QColor(168, 136, 84), 1.4));
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace PieceRenderer
