#include "Marks.h"

#include "PdfFile.h"
#include "Signature.h"

#include <poppler-annotation.h>
#include <poppler-qt6.h>

#include <QUuid>

namespace pdf {

QString Marks::namePrefix() { return QStringLiteral("goodinput-"); }

static QString newName()
{
    return Marks::namePrefix() + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QList<QColor> Marks::highlightColours()
{
    // Yellow, then the Visual sheet's bright teal, blue and lavender.
    return {QColor(0xFF, 0xD6, 0x0A), QColor(0x5C, 0xEB, 0xD4), QColor(0xA5, 0xC3, 0xFF),
            QColor(0xDC, 0xC2, 0xFF)};
}

Marks::Marks(PdfFile &file)
    : m_file(file)
{
}

QString Marks::addHighlight(int page, const QList<QRectF> &rects, const QColor &color)
{
    const auto p = m_file.page(page);
    if (!p || rects.isEmpty() || !color.isValid())
        return {};

    Poppler::HighlightAnnotation mark;
    QList<Poppler::HighlightAnnotation::Quad> quads;
    QRectF bounds;
    for (const QRectF &r : rects) {
        const QRectF clipped = r.intersected(QRectF(0, 0, 1, 1));
        if (clipped.isEmpty())
            continue;
        Poppler::HighlightAnnotation::Quad quad;
        quad.points[0] = clipped.topLeft();
        quad.points[1] = clipped.topRight();
        quad.points[2] = clipped.bottomRight();
        quad.points[3] = clipped.bottomLeft();
        quad.capStart = false;
        quad.capEnd = false;
        quad.feather = 0.1;
        quads.append(quad);
        bounds = bounds.united(clipped);
    }
    if (quads.isEmpty())
        return {};
    mark.setHighlightType(Poppler::HighlightAnnotation::Highlight);
    mark.setHighlightQuads(quads);
    mark.setBoundary(bounds);
    Poppler::Annotation::Style style;
    style.setColor(color);
    style.setOpacity(1.0);
    mark.setStyle(style);
    const QString name = newName();
    mark.setUniqueName(name);
    p->addAnnotation(&mark);
    m_changed = true;
    return name;
}

QList<Marks::Highlight> Marks::highlights(int page) const
{
    QList<Highlight> out;
    const auto p = m_file.page(page);
    if (!p)
        return out;
    for (const auto &annotation : p->annotations({Poppler::Annotation::AHighlight})) {
        const auto &mark = static_cast<const Poppler::HighlightAnnotation &>(*annotation);
        Highlight h{mark.uniqueName(), page, {}, mark.style().color()};
        for (const auto &quad : mark.highlightQuads())
            h.rects.append(QRectF(quad.points[0], quad.points[2]).normalized());
        out.append(h);
    }
    return out;
}

bool Marks::remove(int page, const QString &name)
{
    const auto p = m_file.page(page);
    if (!p || name.isEmpty())
        return false;
    for (auto &annotation : p->annotations()) {
        if (annotation->uniqueName() != name)
            continue;
        // removeAnnotation destroys the object it is given.
        p->removeAnnotation(annotation.release());
        m_changed = true;
        return true;
    }
    return false;
}

QString Marks::placeSignature(int page, const QRectF &area, const SignatureInk &ink)
{
    const auto p = m_file.page(page);
    const QSizeF pageSize = m_file.pageSize(page);
    if (!p || ink.isEmpty() || area.isEmpty() || pageSize.isEmpty())
        return {};

    // Fit the ink, drawn on a pad of `ink.aspect`, into the area as seen on
    // the real page, then back into the page's normalised space.
    const double areaW = area.width() * pageSize.width();
    const double areaH = area.height() * pageSize.height();
    double w = areaW;
    double h = w / ink.aspect;
    if (h > areaH) {
        h = areaH;
        w = h * ink.aspect;
    }
    const QRectF target((area.center().x() * pageSize.width() - w / 2) / pageSize.width(),
                        (area.center().y() * pageSize.height() - h / 2) / pageSize.height(),
                        w / pageSize.width(), h / pageSize.height());

    QList<QVector<QPointF>> paths;
    for (const auto &stroke : ink.strokes) {
        QVector<QPointF> path;
        path.reserve(stroke.size());
        for (const QPointF &pt : stroke)
            path.append(QPointF(target.x() + pt.x() * target.width(),
                                target.y() + pt.y() * target.height()));
        if (path.size() > 1)
            paths.append(path);
    }
    if (paths.isEmpty())
        return {};

    Poppler::InkAnnotation mark;
    mark.setInkPaths(paths);
    mark.setBoundary(target);
    Poppler::Annotation::Style style;
    style.setColor(QColor(0x14, 0x14, 0x14));
    style.setWidth(1.5);
    mark.setStyle(style);
    const QString name = newName();
    mark.setUniqueName(name);
    p->addAnnotation(&mark);
    m_changed = true;
    return name;
}

} // namespace pdf
