#pragma once

#include <QHash>
#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QSet>
#include <QtQml/qqmlregistration.h>

#include "OfficeDocument.h"

// Shows a window onto a document's pages. The item stays the size of the
// window; contentX and contentY say where in the document it looks, in pixels
// at the current zoom. The engine draws the pages in square tiles, which are
// kept until the engine says an area changed, so scrolling back is instant.
// Keys and input-method text typed while it has focus go to the document.
class PageView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(OfficeDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int part READ part WRITE setPart NOTIFY partChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(qreal contentX READ contentX WRITE setContentX NOTIFY viewChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY viewChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY contentSizeChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentSizeChanged)
    Q_PROPERTY(int pendingTiles READ pendingTiles NOTIFY pendingTilesChanged)
    // How wide one of the engine's pixels is drawn here, for what the engine
    // reports in its own pixels (a sheet's row and column headers).
    Q_PROPERTY(qreal pixelScale READ pixelScale NOTIFY pixelScaleChanged)

public:
    explicit PageView(QQuickItem *parent = nullptr);

    OfficeDocument *document() const { return m_document; }
    void setDocument(OfficeDocument *document);
    // Which part (sheet, slide) to show; -1 follows the one being edited.
    int part() const { return m_part; }
    void setPart(int part);
    // Pixels per twip.
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal zoom);
    qreal contentX() const { return m_contentX; }
    void setContentX(qreal x);
    qreal contentY() const { return m_contentY; }
    void setContentY(qreal y);
    qreal contentWidth() const;
    qreal contentHeight() const;
    int pendingTiles() const { return m_pending.size(); }
    qreal pixelScale() const { return m_pixelScale; }

    // From a point on this item to the document, and back.
    Q_INVOKABLE QPointF toDocument(qreal x, qreal y) const;
    Q_INVOKABLE QRectF toView(const QRectF &twips) const;

    // The selection to and from the desktop's clipboard.
    Q_INVOKABLE void copySelection(bool cut = false);
    Q_INVOKABLE void paste();

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void documentChanged();
    void partChanged();
    void zoomChanged();
    void viewChanged();
    void contentSizeChanged();
    void pendingTilesChanged();
    void pixelScaleChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    struct Tile
    {
        QImage image;
        bool stale = false;
    };
    static quint64 key(int part, int column, int row);
    // A tile's side in twips, a whole number so that the engine's grid and
    // the tiles agree exactly.
    int tileTwips() const;
    qreal tileSide() const { return tileTwips() * m_zoom; }
    void dropTiles();
    void invalidate(const QRectF &area, int part);
    void request(int part, int column, int row, int tilePixels, qreal twipsPerTile);

    QPointer<OfficeDocument> m_document;
    int m_part = -1;
    qreal m_zoom = 0.05;
    qreal m_contentX = 0;
    qreal m_contentY = 0;
    QHash<quint64, Tile> m_tiles;
    QSet<quint64> m_pending;
    int m_generation = 0;
    qreal m_pixelScale = 1;
    // The tile pixels and twips last told to the engine, so a sheet's
    // cursor, selection and headers line up with the tiles drawn.
    QSize m_clientZoom;
};
