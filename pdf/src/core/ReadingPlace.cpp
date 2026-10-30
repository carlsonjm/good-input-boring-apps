#include "ReadingPlace.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

#include <algorithm>
#include <memory>

namespace pdf {

static QString keyFor(const QString &path)
{
    const QByteArray absolute = QFileInfo(path).absoluteFilePath().toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(absolute, QCryptographicHash::Sha256).toHex().left(32));
}

static std::unique_ptr<QSettings> open(const QString &file)
{
    return std::make_unique<QSettings>(file, QSettings::IniFormat);
}

ReadingPlace::ReadingPlace(const QString &settingsFile)
    : m_file(settingsFile.isEmpty()
                 ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                       .filePath(QStringLiteral("places.ini"))
                 : settingsFile)
{
}

int ReadingPlace::page(const QString &path) const
{
    auto settings = open(m_file);
    return std::max(0, settings->value(QStringLiteral("places/%1/page").arg(keyFor(path)), 0).toInt());
}

void ReadingPlace::remember(const QString &path, int page)
{
    auto settings = open(m_file);
    settings->beginGroup(QStringLiteral("places"));
    settings->setValue(keyFor(path) + QStringLiteral("/page"), std::max(0, page));
    settings->setValue(keyFor(path) + QStringLiteral("/at"), QDateTime::currentSecsSinceEpoch());

    QStringList keys = settings->childGroups();
    if (keys.size() > kept) {
        std::sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
            return settings->value(a + QStringLiteral("/at")).toLongLong()
                   < settings->value(b + QStringLiteral("/at")).toLongLong();
        });
        for (int i = 0; i < keys.size() - kept; ++i)
            settings->remove(keys.at(i));
    }
    settings->endGroup();
}

} // namespace pdf
