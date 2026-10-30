#pragma once

#include <QQuickImageProvider>

// Lucide icons in any ink: "image://glyph/<name>/<#rrggbb or #aarrggbb>",
// drawn crisp at the size asked for. The SVGs ship in the app's resources.
class Glyphs : public QQuickImageProvider
{
public:
    Glyphs();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
