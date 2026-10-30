#pragma once

#include <QColor>
#include <QObject>

// The Visual sheet's colour roles, in Light or Dark as the desktop is set.
// Dark values are the sheet's own; Light lays its ink over its ground in the
// same steps. The page itself is white paper in both: only the desk changes.
class Look : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor desk READ desk NOTIFY changed)        // around the page
    Q_PROPERTY(QColor chrome READ chrome NOTIFY changed)    // the bar, menus, sheets
    Q_PROPERTY(QColor raised READ raised NOTIFY changed)    // a resting filled control
    Q_PROPERTY(QColor ink READ ink NOTIFY changed)          // text and glyphs
    Q_PROPERTY(QColor softInk READ softInk NOTIFY changed)  // supporting words
    Q_PROPERTY(QColor divider READ divider NOTIFY changed)
    Q_PROPERTY(QColor strongBorder READ strongBorder NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)    // focus and selection on paper
    Q_PROPERTY(QColor shadow READ shadow NOTIFY changed)

public:
    explicit Look(QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    QColor desk() const;
    QColor chrome() const;
    QColor raised() const;
    QColor ink() const;
    QColor softInk() const;
    QColor divider() const;
    QColor strongBorder() const;
    QColor accent() const;
    QColor shadow() const;

    // Ink at a given strength, for the sheet's hover (0.12), pressed (0.16)
    // and selected (0.24) fills.
    Q_INVOKABLE QColor fill(double strength) const;

Q_SIGNALS:
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void read();
    bool m_dark = true;
    QColor m_accent;
};
