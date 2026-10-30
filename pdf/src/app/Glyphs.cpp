#include "Glyphs.h"

#include <QColor>
#include <QFile>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>

Glyphs::Glyphs()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage Glyphs::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int slash = id.indexOf(QLatin1Char('/'));
    const QString name = id.left(slash);
    const QColor ink(slash > 0 ? id.mid(slash + 1) : QStringLiteral("#000000"));
    static const QRegularExpression safe(QStringLiteral("^[a-z-]+$"));
    if (!safe.match(name).hasMatch() || !ink.isValid())
        return {};

    QFile file(QStringLiteral(":/qt/qml/GoodInputPdf/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = file.readAll();
    svg.replace("currentColor", ink.name(QColor::HexRgb).toLatin1());
    if (ink.alpha() < 255)
        svg.replace("<svg ", QByteArray("<svg opacity=\"") + QByteArray::number(ink.alphaF(), 'f', 3) + "\" ");

    const QSize out = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(24, 24);
    QImage image(out, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    if (size)
        *size = out;
    return image;
}
