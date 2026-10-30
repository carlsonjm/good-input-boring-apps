#pragma once

#include <QString>

namespace pdf {

// Remembers the page each PDF was left on, so it opens there next time. Only
// the page number and a fingerprint of the path are kept, never the file name.
class ReadingPlace
{
public:
    // `settingsFile` empty means the app's own settings.
    explicit ReadingPlace(const QString &settingsFile = {});

    int page(const QString &path) const;   // 0 when never seen
    void remember(const QString &path, int page);

    static constexpr int kept = 200;       // oldest places are dropped past this

private:
    QString m_file;
};

} // namespace pdf
