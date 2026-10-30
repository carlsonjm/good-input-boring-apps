#include "Recent.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr int Kept = 60;

const char BlankText[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text"><office:body><office:text><text:p/></office:text></office:body></office:document>
)";

const char BlankSheet[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.spreadsheet"><office:body><office:spreadsheet><table:table table:name="Sheet 1"><table:table-row><table:table-cell/></table:table-row></table:table></office:spreadsheet></office:body></office:document>
)";

const char BlankSlides[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:presentation="urn:oasis:names:tc:opendocument:xmlns:presentation:1.0" xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.presentation"><office:body><office:presentation><draw:page draw:name="Slide 1"><draw:frame presentation:class="title" svg:x="2cm" svg:y="6cm" svg:width="24cm" svg:height="3cm"><draw:text-box><text:p/></draw:text-box></draw:frame></draw:page></office:presentation></office:body></office:document>
)";

} // namespace

Recent::Recent(QObject *parent)
    : QAbstractListModel(parent)
{
    reload();
}

QString Recent::storePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/good-input-office/recent.json");
}

QStringList Recent::suffixes() const
{
    if (m_kind == QLatin1String("spreadsheet")) {
        return {QStringLiteral("xlsx"), QStringLiteral("xls"), QStringLiteral("ods"), QStringLiteral("csv"), QStringLiteral("fods")};
    }
    if (m_kind == QLatin1String("presentation")) {
        return {QStringLiteral("pptx"), QStringLiteral("ppt"), QStringLiteral("odp"), QStringLiteral("fodp")};
    }
    return {QStringLiteral("docx"), QStringLiteral("doc"), QStringLiteral("odt"), QStringLiteral("rtf"), QStringLiteral("txt"), QStringLiteral("fodt")};
}

bool Recent::accepts(const QUrl &url) const
{
    return suffixes().contains(QFileInfo(url.toLocalFile()).suffix().toLower());
}

void Recent::setKind(const QString &kind)
{
    if (kind == m_kind) {
        return;
    }
    m_kind = kind;
    Q_EMIT kindChanged();
    reload();
}

void Recent::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT filterChanged();
    reload();
}

QList<Recent::Entry> Recent::readAll() const
{
    QFile file(storePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QList<Entry> entries;
    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        Entry entry{QUrl(object.value(QStringLiteral("url")).toString()),
                    QDateTime::fromString(object.value(QStringLiteral("when")).toString(), Qt::ISODate)};
        if (entry.url.isValid()) {
            entries << entry;
        }
    }
    return entries;
}

void Recent::writeAll(const QList<Entry> &entries) const
{
    QDir().mkpath(QFileInfo(storePath()).path());
    QJsonArray array;
    for (const Entry &entry : entries) {
        array.append(QJsonObject{{QStringLiteral("url"), entry.url.toString()},
                                 {QStringLiteral("when"), entry.when.toString(Qt::ISODate)}});
    }
    QSaveFile file(storePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson());
        file.commit();
    }
}

void Recent::add(const QUrl &url)
{
    if (!url.isLocalFile() || url.toLocalFile().startsWith(QDir::tempPath())) {
        return;
    }
    QList<Entry> entries = readAll();
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Entry &e) { return e.url == url; }),
                  entries.end());
    entries.prepend({url, QDateTime::currentDateTimeUtc()});
    while (entries.size() > Kept) {
        entries.removeLast();
    }
    writeAll(entries);
    reload();
}

void Recent::reload()
{
    beginResetModel();
    m_shown.clear();
    for (const Entry &entry : readAll()) {
        const QFileInfo info(entry.url.toLocalFile());
        if (!info.exists() || !accepts(entry.url)) {
            continue;
        }
        if (!m_filter.isEmpty() && !info.completeBaseName().contains(m_filter, Qt::CaseInsensitive)) {
            continue;
        }
        m_shown << entry;
    }
    endResetModel();
    Q_EMIT countChanged();
}

int Recent::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_shown.size());
}

QString Recent::describeWhen(const QDateTime &when, const QDateTime &now)
{
    const qint64 seconds = when.secsTo(now);
    if (seconds < 60) {
        return QStringLiteral("Just now");
    }
    if (seconds < 3600) {
        const qint64 minutes = seconds / 60;
        return minutes == 1 ? QStringLiteral("1 minute ago") : QStringLiteral("%1 minutes ago").arg(minutes);
    }
    const QDate day = when.toLocalTime().date();
    const QDate today = now.toLocalTime().date();
    if (day == today) {
        return QStringLiteral("Today");
    }
    if (day.addDays(1) == today) {
        return QStringLiteral("Yesterday");
    }
    if (day.daysTo(today) < 7) {
        return QLocale().dayName(day.dayOfWeek());
    }
    return QLocale().toString(day, QStringLiteral("d MMM"));
}

QVariant Recent::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size()) {
        return {};
    }
    const Entry &entry = m_shown.at(index.row());
    const QFileInfo info(entry.url.toLocalFile());
    switch (role) {
    case NameRole:
        return info.completeBaseName();
    case UrlRole:
        return entry.url;
    case PlaceRole: {
        QString place = QDir::home().relativeFilePath(info.path());
        if (place == QLatin1String(".")) {
            place = QStringLiteral("Home");
        }
        const QString suffix = info.suffix().toLower();
        if (suffix == QLatin1String("docx") || suffix == QLatin1String("doc")) {
            place += QStringLiteral(" · Word file");
        } else if (suffix == QLatin1String("xlsx") || suffix == QLatin1String("xls")) {
            place += QStringLiteral(" · Excel file");
        } else if (suffix == QLatin1String("pptx") || suffix == QLatin1String("ppt")) {
            place += QStringLiteral(" · PowerPoint file");
        }
        return place;
    }
    case WhenRole:
        return describeWhen(entry.when, QDateTime::currentDateTimeUtc());
    default:
        return {};
    }
}

QHash<int, QByteArray> Recent::roleNames() const
{
    return {{NameRole, "name"}, {UrlRole, "url"}, {PlaceRole, "place"}, {WhenRole, "when"}};
}

QUrl Recent::blankSource() const
{
    QString suffix = QStringLiteral("fodt");
    const char *content = BlankText;
    if (m_kind == QLatin1String("spreadsheet")) {
        suffix = QStringLiteral("fods");
        content = BlankSheet;
    } else if (m_kind == QLatin1String("presentation")) {
        suffix = QStringLiteral("fodp");
        content = BlankSlides;
    }
    const QString path = QDir::tempPath() + QStringLiteral("/good-input-office-blank.") + suffix;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(content);
    }
    return QUrl::fromLocalFile(path);
}

QUrl Recent::templateSource(const QString &name) const
{
    QFile source(QStringLiteral(":/templates/") + name + QStringLiteral(".fods"));
    if (!source.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QString path = QDir::tempPath() + QStringLiteral("/good-input-office-template.fods");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    file.write(source.readAll());
    return QUrl::fromLocalFile(path);
}

QUrl Recent::newDocumentTarget(const QString &name) const
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    QDir().mkpath(folder);
    QString base = QStringLiteral("Untitled document");
    QString suffix = QStringLiteral("docx");
    if (m_kind == QLatin1String("spreadsheet")) {
        base = QStringLiteral("Untitled spreadsheet");
        suffix = QStringLiteral("xlsx");
    } else if (m_kind == QLatin1String("presentation")) {
        base = QStringLiteral("Untitled presentation");
        suffix = QStringLiteral("pptx");
    }
    if (!name.isEmpty()) {
        base = name;
    }
    QString path = QStringLiteral("%1/%2.%3").arg(folder, base, suffix);
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = QStringLiteral("%1/%2 %3.%4").arg(folder, base).arg(n).arg(suffix);
    }
    return QUrl::fromLocalFile(path);
}
