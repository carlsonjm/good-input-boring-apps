#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// The documents a person opened lately, newest first, for one kind of app
// (Docs shows text, Sheets spreadsheets, Slides presentations). The list is
// this program's own; files that have gone are left out when it is read.
class Recent : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, UrlRole, PlaceRole, WhenRole };

    explicit Recent(QObject *parent = nullptr);

    QString kind() const { return m_kind; }
    void setKind(const QString &kind);
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void add(const QUrl &url);
    Q_INVOKABLE void reload();
    // A blank file of this app's kind to open, and where it will be kept.
    Q_INVOKABLE QUrl blankSource() const;
    // A name may be given in place of "Untitled spreadsheet" and the like.
    Q_INVOKABLE QUrl newDocumentTarget(const QString &name = {}) const;
    // One of the ready-made files built into the program ("budget",
    // "tracker", "shopping", "schedule"), copied out to open.
    Q_INVOKABLE QUrl templateSource(const QString &name) const;
    // Whether a file belongs to this app's kind, by its name.
    Q_INVOKABLE bool accepts(const QUrl &url) const;

    // For tests: where the list lives.
    static QString storePath();
    static QString describeWhen(const QDateTime &when, const QDateTime &now);

Q_SIGNALS:
    void kindChanged();
    void filterChanged();
    void countChanged();

private:
    struct Entry
    {
        QUrl url;
        QDateTime when;
    };
    QList<Entry> readAll() const;
    void writeAll(const QList<Entry> &entries) const;
    QStringList suffixes() const;

    QString m_kind = QStringLiteral("text");
    QString m_filter;
    QList<Entry> m_shown;
};
