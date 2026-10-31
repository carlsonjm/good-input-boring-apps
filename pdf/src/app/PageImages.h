#pragma once

#include <QQuickImageProvider>

class Session;

// Page pictures for QML: "image://page/<revision>/<page>", drawn at the width
// asked for by sourceSize. The revision is Session's, so a changed page is a
// new address and never comes from a stale cache.
class PageImages : public QQuickImageProvider
{
public:
    explicit PageImages(const Session &session);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    static constexpr int widest = 4096; // never draw a page wider than this

private:
    const Session &m_session;
};
