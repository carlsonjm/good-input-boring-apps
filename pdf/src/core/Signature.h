#pragma once

#include <QList>
#include <QPointF>
#include <QString>

namespace pdf {

// A drawn signature: strokes normalised to the pad they were drawn on, 0 to 1
// on both axes, and that pad's width over its height, so it can be laid on a
// page without stretching.
struct SignatureInk {
    QList<QList<QPointF>> strokes;
    double aspect = 2.6;

    bool isEmpty() const;
    // Drops points closer together than a finger can mean, and strokes too
    // short to be ink.
    SignatureInk simplified() const;
};

// Keeps the person's signature on this computer only, in a file only they
// can read, and nowhere else.
class SignatureStore
{
public:
    // `directory` empty means the app's own data folder.
    explicit SignatureStore(const QString &directory = {});

    bool has() const;
    SignatureInk load() const;
    bool save(const SignatureInk &ink) const;
    bool forget() const;

    QString filePath() const;

private:
    QString m_directory;
};

} // namespace pdf
