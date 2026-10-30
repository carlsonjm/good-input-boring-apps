#include "Look.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

Look::Look(QObject *parent)
    : QObject(parent)
{
    read();
    qGuiApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { read(); });
#endif
}

bool Look::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qGuiApp && event->type() == QEvent::ApplicationPaletteChange)
        read();
    return QObject::eventFilter(watched, event);
}

void Look::read()
{
    const QPalette palette = QGuiApplication::palette();
    bool dark = palette.color(QPalette::Window).lightnessF() < 0.5;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown)
        dark = scheme == Qt::ColorScheme::Dark;
#endif
    const QColor accent = palette.color(QPalette::Highlight);
    if (dark == m_dark && accent == m_accent)
        return;
    m_dark = dark;
    m_accent = accent;
    Q_EMIT changed();
}

static QColor withAlpha(QColor c, double a)
{
    c.setAlphaF(float(a));
    return c;
}

QColor Look::desk() const { return m_dark ? QColor(0x14, 0x14, 0x14) : QColor(0xE6, 0xE6, 0xEC); }
QColor Look::chrome() const { return m_dark ? QColor(0x1C, 0x1C, 0x1C) : QColor(0xF8, 0xF8, 0xFF); }
QColor Look::raised() const { return m_dark ? QColor(0x24, 0x24, 0x24) : QColor(0xFF, 0xFF, 0xFF); }
QColor Look::ink() const { return m_dark ? QColor(0xF8, 0xF8, 0xFF) : QColor(0x14, 0x14, 0x14); }
QColor Look::softInk() const { return withAlpha(ink(), m_dark ? 0.66 : 0.66); }
QColor Look::divider() const { return m_dark ? QColor(0x33, 0x33, 0x33) : QColor(0xD6, 0xD6, 0xDE); }
QColor Look::strongBorder() const { return m_dark ? QColor(0x5A, 0x5A, 0x5A) : QColor(0xA0, 0xA0, 0xA8); }
QColor Look::shadow() const { return withAlpha(QColor(0, 0, 0), m_dark ? 0.35 : 0.16); }
QColor Look::fill(double strength) const { return withAlpha(ink(), strength); }

QColor Look::accent() const
{
    // On white paper the accent must read; a pale or grey one falls back to
    // Breeze's blue, as the mock-up draws selections and focus.
    if (m_accent.isValid() && m_accent.saturationF() > 0.25 && m_accent.lightnessF() < 0.75)
        return m_accent;
    return QColor(0x3D, 0xAE, 0xE9);
}
