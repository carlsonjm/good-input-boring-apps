#include "PageView.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QQuickWindow>

#include <cmath>

namespace {
// A tile is this many logical pixels square at any zoom.
constexpr int TileSize = 256;
// Past this many tiles the ones furthest from view are let go.
constexpr int MaxTiles = 400;
} // namespace

PageView::PageView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setFlag(ItemAcceptsInputMethod, true);
    setAcceptedMouseButtons(Qt::NoButton);
    setOpaquePainting(false);
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { update(); });
}

quint64 PageView::key(int part, int column, int row)
{
    return (quint64(quint16(part)) << 48) | (quint64(quint32(column) & 0xffffff) << 24) | (quint32(row) & 0xffffff);
}

void PageView::setDocument(OfficeDocument *document)
{
    if (document == m_document) {
        return;
    }
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = document;
    dropTiles();
    if (m_document) {
        connect(m_document, &OfficeDocument::invalidated, this, &PageView::invalidate);
        connect(m_document, &OfficeDocument::sizeChanged, this, &PageView::contentSizeChanged);
        connect(m_document, &OfficeDocument::statusChanged, this, [this] {
            dropTiles();
            Q_EMIT contentSizeChanged();
        });
        connect(m_document, &OfficeDocument::partsChanged, this, [this] { update(); });
        connect(m_document, &OfficeDocument::cursorChanged, this, [this] {
            QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
        });
    }
    Q_EMIT documentChanged();
    Q_EMIT contentSizeChanged();
}

void PageView::setPart(int part)
{
    if (part == m_part) {
        return;
    }
    m_part = part;
    Q_EMIT partChanged();
    update();
}

void PageView::setZoom(qreal zoom)
{
    zoom = qBound(0.005, zoom, 0.5);
    if (qFuzzyCompare(zoom, m_zoom)) {
        return;
    }
    m_zoom = zoom;
    ++m_generation;
    m_tiles.clear();
    m_pending.clear();
    Q_EMIT zoomChanged();
    Q_EMIT contentSizeChanged();
    update();
}

void PageView::setContentX(qreal x)
{
    if (qFuzzyCompare(x + 1, m_contentX + 1)) {
        return;
    }
    m_contentX = x;
    Q_EMIT viewChanged();
    update();
}

void PageView::setContentY(qreal y)
{
    if (qFuzzyCompare(y + 1, m_contentY + 1)) {
        return;
    }
    m_contentY = y;
    Q_EMIT viewChanged();
    update();
}

int PageView::tileTwips() const
{
    return qMax(1, qRound(TileSize / m_zoom));
}

qreal PageView::contentWidth() const
{
    return m_document ? m_document->size().width() * m_zoom : 0;
}

qreal PageView::contentHeight() const
{
    return m_document ? m_document->size().height() * m_zoom : 0;
}

QPointF PageView::toDocument(qreal x, qreal y) const
{
    return QPointF((x + m_contentX) / m_zoom, (y + m_contentY) / m_zoom);
}

QRectF PageView::toView(const QRectF &twips) const
{
    return QRectF(twips.x() * m_zoom - m_contentX, twips.y() * m_zoom - m_contentY,
                  twips.width() * m_zoom, twips.height() * m_zoom);
}

void PageView::dropTiles()
{
    m_clientZoom = QSize();
    ++m_generation;
    m_tiles.clear();
    m_pending.clear();
    Q_EMIT pendingTilesChanged();
    update();
}

void PageView::invalidate(const QRectF &area, int part)
{
    if (area.isEmpty()) {
        for (auto it = m_tiles.begin(); it != m_tiles.end(); ++it) {
            if (int(it.key() >> 48) == part) {
                it->stale = true;
            }
        }
    } else {
        const qreal span = tileTwips();
        const int c0 = int(std::floor(area.left() / span));
        const int c1 = int(std::floor(area.right() / span));
        const int r0 = int(std::floor(area.top() / span));
        const int r1 = int(std::floor(area.bottom() / span));
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                auto it = m_tiles.find(key(part, c, r));
                if (it != m_tiles.end()) {
                    it->stale = true;
                }
            }
        }
    }
    update();
}

void PageView::request(int part, int column, int row, int tilePixels, qreal twipsPerTile)
{
    const quint64 k = key(part, column, row);
    if (m_pending.contains(k)) {
        return;
    }
    m_pending.insert(k);
    Q_EMIT pendingTilesChanged();
    const int generation = m_generation;
    const QRectF area(column * twipsPerTile, row * twipsPerTile, twipsPerTile, twipsPerTile);
    m_document->renderTile(part, area, QSize(tilePixels, tilePixels), this, [this, k, generation](QImage image) {
        if (generation != m_generation) {
            return;
        }
        m_pending.remove(k);
        Tile &tile = m_tiles[k];
        tile.image = std::move(image);
        tile.stale = false;
        Q_EMIT pendingTilesChanged();
        update();
    });
}

void PageView::paint(QPainter *painter)
{
    if (!m_document || m_document->status() != OfficeDocument::Ready) {
        return;
    }
    const int part = m_part >= 0 ? m_part : m_document->currentPart();
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int tilePixels = qMax(64, int(std::ceil(TileSize * dpr)));
    const int twipsPerTile = tileTwips();
    const qreal side = tileSide();
    const qreal toSource = tilePixels / side;
    const qreal maxX = contentWidth();
    const qreal maxY = contentHeight();

    // A sheet's engine reports its cursor, selection and headers at the zoom
    // it is told; at any other it rounds each row and column differently
    // from the tiles, and the ring drifts off the grid further down.
    if (m_document->kind() == OfficeDocument::Spreadsheet && m_part < 0
        && m_clientZoom != QSize(tilePixels, twipsPerTile)) {
        m_clientZoom = QSize(tilePixels, twipsPerTile);
        m_document->setClientZoom(tilePixels, twipsPerTile);
    }
    if (!qFuzzyCompare(m_pixelScale, 1 / toSource)) {
        m_pixelScale = 1 / toSource;
        Q_EMIT pixelScaleChanged();
    }

    const int c0 = qMax(0, int(std::floor(m_contentX / side)));
    const int r0 = qMax(0, int(std::floor(m_contentY / side)));
    const int c1 = int(std::floor(qMin(maxX, m_contentX + width()) / side));
    const int r1 = int(std::floor(qMin(maxY, m_contentY + height()) / side));

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const QRectF target(c * side - m_contentX, r * side - m_contentY, side, side);
            const QRectF clipped = target.intersected(QRectF(-m_contentX, -m_contentY, maxX, maxY));
            if (clipped.isEmpty()) {
                continue;
            }
            const quint64 k = key(part, c, r);
            auto it = m_tiles.constFind(k);
            if (it != m_tiles.constEnd() && !it->image.isNull()) {
                const QRectF source((clipped.x() - target.x()) * toSource, (clipped.y() - target.y()) * toSource,
                                    clipped.width() * toSource, clipped.height() * toSource);
                painter->drawImage(clipped, it->image, source);
            }
            if (it == m_tiles.constEnd() || it->stale) {
                request(part, c, r, tilePixels, twipsPerTile);
            }
        }
    }

    // Let go of the tiles furthest from where the person is looking.
    if (m_tiles.size() > MaxTiles) {
        const QPointF centre((m_contentX + width() / 2) / side, (m_contentY + height() / 2) / side);
        QList<QPair<qreal, quint64>> byDistance;
        for (auto it = m_tiles.cbegin(); it != m_tiles.cend(); ++it) {
            const int c = int((it.key() >> 24) & 0xffffff);
            const int r = int(it.key() & 0xffffff);
            byDistance.append({std::hypot(c - centre.x(), r - centre.y()), it.key()});
        }
        std::sort(byDistance.begin(), byDistance.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        for (int i = 0; i < m_tiles.size() - MaxTiles && i < byDistance.size(); ++i) {
            m_tiles.remove(byDistance[i].second);
        }
    }
}

void PageView::copySelection(bool cut)
{
    if (!m_document) {
        return;
    }
    const QString text = m_document->selectedText();
    if (text.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(text);
    if (cut) {
        m_document->pressKey(OfficeDocument::KeyDelete);
    }
}

void PageView::paste()
{
    if (!m_document) {
        return;
    }
    const QString text = QGuiApplication::clipboard()->text();
    if (!text.isEmpty()) {
        m_document->pasteText(text);
    }
}

void PageView::keyPressEvent(QKeyEvent *event)
{
    if (!m_document || m_document->status() != OfficeDocument::Ready) {
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }
    const Qt::KeyboardModifiers mods = event->modifiers();
    int engineMods = 0;
    if (mods & Qt::ShiftModifier) {
        engineMods |= OfficeDocument::Shift;
    }
    if (mods & Qt::ControlModifier) {
        engineMods |= OfficeDocument::Control;
    }
    if (mods & Qt::AltModifier) {
        engineMods |= OfficeDocument::Alt;
    }

    if (mods & Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_B: m_document->command(QStringLiteral(".uno:Bold")); return;
        case Qt::Key_I: m_document->command(QStringLiteral(".uno:Italic")); return;
        case Qt::Key_U: m_document->command(QStringLiteral(".uno:Underline")); return;
        case Qt::Key_Z:
            if (mods & Qt::ShiftModifier) {
                m_document->redo();
            } else {
                m_document->undo();
            }
            return;
        case Qt::Key_Y: m_document->redo(); return;
        case Qt::Key_A: m_document->selectAll(); return;
        case Qt::Key_C: copySelection(false); return;
        case Qt::Key_X: copySelection(true); return;
        case Qt::Key_V: paste(); return;
        case Qt::Key_S: m_document->save(); return;
        default: break;
        }
    }

    int engineKey = 0;
    switch (event->key()) {
    case Qt::Key_Down: engineKey = OfficeDocument::KeyDown; break;
    case Qt::Key_Up: engineKey = OfficeDocument::KeyUp; break;
    case Qt::Key_Left: engineKey = OfficeDocument::KeyLeft; break;
    case Qt::Key_Right: engineKey = OfficeDocument::KeyRight; break;
    case Qt::Key_Home: engineKey = OfficeDocument::KeyHome; break;
    case Qt::Key_End: engineKey = OfficeDocument::KeyEnd; break;
    case Qt::Key_PageUp: engineKey = OfficeDocument::KeyPageUp; break;
    case Qt::Key_PageDown: engineKey = OfficeDocument::KeyPageDown; break;
    case Qt::Key_Return:
    case Qt::Key_Enter: engineKey = OfficeDocument::KeyReturn; break;
    case Qt::Key_Escape: engineKey = OfficeDocument::KeyEscape; break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: engineKey = OfficeDocument::KeyTab; break;
    case Qt::Key_Backspace: engineKey = OfficeDocument::KeyBackspace; break;
    case Qt::Key_Delete: engineKey = OfficeDocument::KeyDelete; break;
    default: break;
    }
    if (engineKey) {
        if (event->key() == Qt::Key_Backtab) {
            engineMods |= OfficeDocument::Shift;
        }
        m_document->pressKey(engineKey, engineMods);
        event->accept();
        return;
    }
    const QString text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint()) {
        m_document->typeText(text);
        event->accept();
        return;
    }
    QQuickPaintedItem::keyPressEvent(event);
}

void PageView::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_document) {
        return;
    }
    // A keyboard that corrects a word replaces the letters before the cursor.
    if (event->replacementLength() > 0 && event->replacementStart() < 0) {
        for (int i = 0; i < event->replacementLength(); ++i) {
            m_document->pressKey(OfficeDocument::KeyBackspace);
        }
    }
    if (!event->commitString().isEmpty()) {
        m_document->typeText(event->commitString());
    }
    event->accept();
}

QVariant PageView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return m_document && m_document->status() == OfficeDocument::Ready;
    case Qt::ImCursorRectangle:
        return m_document ? toView(m_document->cursorRect()) : QRectF();
    case Qt::ImHints:
        return int(Qt::ImhMultiLine);
    case Qt::ImSurroundingText:
        return QString();
    case Qt::ImCursorPosition:
    case Qt::ImAnchorPosition:
        return 0;
    default:
        return QQuickPaintedItem::inputMethodQuery(query);
    }
}
