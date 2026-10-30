#include "PageImages.h"

#include "Session.h"

PageImages::PageImages(const Session &session)
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_session(session)
{
}

QImage PageImages::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    bool ok = false;
    const int page = parts.value(1).toInt(&ok);
    if (parts.size() != 2 || !ok)
        return {};
    const int width = std::clamp(requestedSize.width() > 0 ? requestedSize.width() : 1200, 16, widest);
    const QImage image = m_session.renderPage(page, width);
    if (size)
        *size = image.size();
    return image;
}
