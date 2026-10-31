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

// A title slide whose boxes say how they look, so no engine's own defaults
// (which newer ones draw filled) show through: no fill, no outline, centred.
const char BlankSlides[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:presentation="urn:oasis:names:tc:opendocument:xmlns:presentation:1.0" xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.presentation"><office:automatic-styles><style:page-layout style:name="PM1"><style:page-layout-properties fo:margin-top="0cm" fo:margin-bottom="0cm" fo:margin-left="0cm" fo:margin-right="0cm" fo:page-width="28cm" fo:page-height="15.75cm" style:print-orientation="landscape"/></style:page-layout><style:style style:name="dp1" style:family="drawing-page"><style:drawing-page-properties draw:fill="solid" draw:fill-color="#ffffff"/></style:style><style:style style:name="pr1" style:family="presentation"><style:graphic-properties draw:fill="none" draw:stroke="none" draw:textarea-vertical-align="bottom"/><style:paragraph-properties fo:text-align="center"/><style:text-properties fo:color="#141414" fo:font-size="40pt"/></style:style><style:style style:name="pr2" style:family="presentation"><style:graphic-properties draw:fill="none" draw:stroke="none" draw:textarea-vertical-align="top"/><style:paragraph-properties fo:text-align="center"/><style:text-properties fo:color="#5A5A60" fo:font-size="22pt"/></style:style></office:automatic-styles><office:master-styles><style:master-page style:name="Default" style:page-layout-name="PM1" draw:style-name="dp1"/></office:master-styles><office:body><office:presentation><draw:page draw:name="Slide 1" draw:master-page-name="Default" draw:style-name="dp1"><draw:frame presentation:style-name="pr1" presentation:class="title" presentation:placeholder="true" svg:x="2cm" svg:y="3.5cm" svg:width="24cm" svg:height="4.5cm"><draw:text-box/></draw:frame><draw:frame presentation:style-name="pr2" presentation:class="subtitle" presentation:placeholder="true" svg:x="2cm" svg:y="8.5cm" svg:width="24cm" svg:height="3cm"><draw:text-box/></draw:frame></draw:page></office:presentation></office:body></office:document>
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
