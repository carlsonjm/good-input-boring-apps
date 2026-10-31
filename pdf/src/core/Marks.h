#pragma once

#include <QColor>
#include <QList>
#include <QRectF>
#include <QString>

namespace pdf {

class PdfFile;
struct SignatureInk;

// What a person adds on top of the page: highlights and a placed signature.
// Each is a standard PDF annotation, so any PDF reader shows it; Poppler
// draws it into the page picture as soon as it is added. Saver writes it.
class Marks
{
public:
    struct Highlight {
        QString name;   // the annotation's unique name
        int page = -1;
        QList<QRectF> rects;
        QColor color;
    };

    explicit Marks(PdfFile &file);

    // The four highlight colours the look offers, in its order.
    static QList<QColor> highlightColours();

    // Returns the new mark's name, or an empty string when nothing was added.
    QString addHighlight(int page, const QList<QRectF> &rects, const QColor &color);
    QList<Highlight> highlights(int page) const;
    bool remove(int page, const QString &name);

    // Lays a signature inside `area` (normalised to the page), as large as
    // fits without stretching it.
    QString placeSignature(int page, const QRectF &area, const SignatureInk &ink);

    bool changed() const { return m_changed; }

    // Every mark this app adds carries this prefix, so ours can be told from
    // marks other readers made.
    static QString namePrefix();

private:
    PdfFile &m_file;
    bool m_changed = false;
};

} // namespace pdf
