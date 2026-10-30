#define LOK_USE_UNSTABLE_API
#include "OfficeDocument.h"

#include "Engine.h"

#include <LibreOfficeKit/LibreOfficeKit.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QThread>

#include <cstdlib>
#include <mutex>

// What the engine thread and the interface thread share about one document.
// The engine calls back from its own threads; the owner is cleared under the
// lock before the document object goes, so a late call back finds nobody.
struct DocumentLink
{
    std::mutex lock;
    OfficeDocument *owner = nullptr;
    lok::Document *doc = nullptr; // touched on the engine thread only

    static void callback(int type, const char *payload, void *data)
    {
        auto *link = static_cast<DocumentLink *>(data);
        const QByteArray text(payload ? payload : "");
        std::lock_guard<std::mutex> guard(link->lock);
        if (OfficeDocument *owner = link->owner) {
            QMetaObject::invokeMethod(owner, [owner, type, text] {
                owner->engineEvent(type, text);
            }, Qt::QueuedConnection);
        }
    }
};

namespace {

// "x, y, w, h" with anything after the fourth number ignored.
bool parseRect(const QByteArray &text, QRectF *rect)
{
    const QList<QByteArray> parts = text.split(',');
    if (parts.size() < 4) {
        return false;
    }
    bool ok[4];
    const double x = parts[0].trimmed().toDouble(&ok[0]);
    const double y = parts[1].trimmed().toDouble(&ok[1]);
    const double w = parts[2].trimmed().toDouble(&ok[2]);
    const double h = parts[3].trimmed().toDouble(&ok[3]);
    if (!(ok[0] && ok[1] && ok[2] && ok[3])) {
        return false;
    }
    *rect = QRectF(x, y, w, h);
    return true;
}

// Newer engines wrap some payloads in JSON; older ones send the bare value.
QByteArray unwrap(const QByteArray &payload, const char *field)
{
    if (!payload.startsWith('{')) {
        return payload;
    }
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    return object.value(QLatin1String(field)).toString().toUtf8();
}

QString takeString(char *text)
{
    if (!text) {
        return {};
    }
    QString result = QString::fromUtf8(text);
    std::free(text);
    return result;
}

QByteArray unoArguments(const QVariantMap &arguments)
{
    if (arguments.isEmpty()) {
        return {};
    }
    QJsonObject object;
    for (auto it = arguments.cbegin(); it != arguments.cend(); ++it) {
        const QVariant &value = it.value();
        QString type = QStringLiteral("string");
        switch (value.typeId()) {
        case QMetaType::Bool:
            type = QStringLiteral("boolean");
            break;
        case QMetaType::Int:
        case QMetaType::LongLong:
            type = QStringLiteral("long");
            break;
        case QMetaType::Double:
            type = QStringLiteral("double");
            break;
        default:
            break;
        }
        object.insert(it.key(), QJsonObject{{QStringLiteral("type"), type},
                                            {QStringLiteral("value"), value.toString()}});
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

OfficeDocument::OfficeDocument(QObject *parent)
    : QObject(parent)
{
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(1500);
    connect(&m_autosaveTimer, &QTimer::timeout, this, [this] {
        if (m_autosave && m_modified && m_status == Ready) {
            save();
        }
    });
}

OfficeDocument::~OfficeDocument()
{
    if (m_autosave && m_modified && m_status == Ready) {
        save();
    }
    close();
}

QString OfficeDocument::title() const
{
    return QFileInfo(m_source.toLocalFile()).completeBaseName();
}

void OfficeDocument::setSource(const QUrl &source)
{
    if (source == m_source) {
        return;
    }
    if (m_autosave && m_modified && m_status == Ready) {
        save();
    }
    close();
    m_source = source;
    Q_EMIT sourceChanged();
    load();
}

void OfficeDocument::setAutosave(bool on)
{
    if (on == m_autosave) {
        return;
    }
    m_autosave = on;
    Q_EMIT autosaveChanged();
}

void OfficeDocument::setStatus(Status status, const QString &error)
{
    m_status = status;
    m_error = error;
    Q_EMIT statusChanged();
}

void OfficeDocument::close()
{
    m_autosaveTimer.stop();
    if (!m_link) {
        return;
    }
    {
        std::lock_guard<std::mutex> guard(m_link->lock);
        m_link->owner = nullptr;
    }
    std::shared_ptr<DocumentLink> link = std::move(m_link);
    Engine::instance()->run([link] {
        delete link->doc;
        link->doc = nullptr;
    });
    m_kind = Unknown;
    m_parts = 0;
    m_currentPart = 0;
    m_partNames.clear();
    m_size = {};
    m_pages.clear();
    m_modified = false;
    m_cursor = {};
    m_cursorVisible = false;
    m_selection.clear();
    m_selectionStart = m_selectionEnd = {};
    m_cellCursor = {};
    m_cellAddress.clear();
    m_cellFormula.clear();
    m_states.clear();
    m_searchRects.clear();
    m_searchCount = 0;
    setStatus(Empty);
}

void OfficeDocument::load()
{
    if (m_source.isEmpty()) {
        return;
    }
    setStatus(Loading);
    auto link = std::make_shared<DocumentLink>();
    link->owner = this;
    m_link = link;
    const QByteArray url = m_source.toString(QUrl::FullyEncoded).toUtf8();
    QPointer<OfficeDocument> self(this);
    Engine::instance()->run([link, url, self] {
        Engine *engine = Engine::instance();
        lok::Office *office = engine->office();
        QString error;
        int type = -1;
        if (!office) {
            error = engine->startError();
        } else {
            link->doc = office->documentLoad(url.constData(), "Language=en-US");
            if (!link->doc) {
                error = takeString(office->getError());
                if (error.isEmpty()) {
                    error = QStringLiteral("The file could not be opened.");
                }
            } else {
                // Off: the engine's own spelling squiggles and margins are kept;
                // the white space between pages stays so pages read as pages.
                link->doc->initializeForRendering(
                    R"({".uno:HideWhitespace":{"type":"boolean","value":"false"},)"
                    R"(".uno:ShowBorderShadow":{"type":"boolean","value":"false"},)"
                    R"(".uno:Author":{"type":"string","value":"You"}})");
                link->doc->registerCallback(&DocumentLink::callback, link.get());
                type = link->doc->getDocumentType();

            }
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, link, error, type] {
            if (!self || self->m_link != link) {
                return;
            }
            if (!error.isEmpty()) {
                self->setStatus(Error, error);
                return;
            }
            switch (type) {
            case LOK_DOCTYPE_TEXT: self->m_kind = Text; break;
            case LOK_DOCTYPE_SPREADSHEET: self->m_kind = Spreadsheet; break;
            case LOK_DOCTYPE_PRESENTATION: self->m_kind = Presentation; break;
            case LOK_DOCTYPE_DRAWING: self->m_kind = Drawing; break;
            default: self->m_kind = Unknown; break;
            }
            // A text document opens with its cursor showing; the engine only
            // reports the cursor's visibility when it changes.
            self->m_cursorVisible = self->m_kind == Text || self->m_kind == Presentation;
            self->refreshShape();
            self->setStatus(Ready);
        }, Qt::QueuedConnection);
    });
}

void OfficeDocument::refreshShape()
{
    if (!m_link) {
        return;
    }
    QSizeF size;
    QList<QRectF> pages;
    int parts = 0;
    int current = 0;
    QStringList names;
    const bool text = m_kind == Text;
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->runAndWait([&] {
        if (!link->doc) {
            return;
        }
        long w = 0;
        long h = 0;
        link->doc->getDocumentSize(&w, &h);
        size = QSizeF(w, h);
        if (text) {
            for (const QByteArray &piece : takeString(link->doc->getPartPageRectangles()).toUtf8().split(';')) {
                QRectF rect;
                if (parseRect(piece, &rect) && !rect.isEmpty()) {
                    pages << rect;
                }
            }
        }
        parts = link->doc->getParts();
        current = link->doc->getPart();
        for (int i = 0; i < parts; ++i) {
            names << takeString(link->doc->getPartName(i));
        }
    });
    if (size != m_size || pages != m_pages) {
        m_size = size;
        m_pages = pages;
        Q_EMIT sizeChanged();
    }
    if (parts != m_parts || current != m_currentPart || names != m_partNames) {
        m_parts = parts;
        m_currentPart = current;
        m_partNames = names;
        Q_EMIT partsChanged();
    }
}

void OfficeDocument::engineEvent(int type, const QByteArray &payload)
{
    if (qEnvironmentVariableIsSet("OFFICE_TRACE")) qDebug() << "event" << type << payload.left(200);
    switch (type) {
    case LOK_CALLBACK_INVALIDATE_TILES: {
        QRectF area;
        int part = m_currentPart;
        if (payload.startsWith("EMPTY")) {
            const QList<QByteArray> parts = payload.split(',');
            if (parts.size() > 1) {
                part = parts[1].trimmed().toInt();
            }
        } else {
            parseRect(payload, &area);
            const QList<QByteArray> parts = payload.split(',');
            if (parts.size() > 4) {
                part = parts[4].trimmed().toInt();
            }
        }
        Q_EMIT invalidated(area, part);
        if (m_modified) {
            m_autosaveTimer.start();
        }
        break;
    }
    case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR: {
        if (qEnvironmentVariableIsSet("OFFICE_TRACE")) qDebug() << "cursor" << payload;
        QRectF rect;
        if (parseRect(unwrap(payload, "rectangle"), &rect)) {
            m_cursor = rect;
            Q_EMIT cursorChanged();
        }
        break;
    }
    case LOK_CALLBACK_CURSOR_VISIBLE: {
        const bool visible = unwrap(payload, "visible") == "true";
        if (visible != m_cursorVisible) {
            m_cursorVisible = visible;
            Q_EMIT cursorChanged();
        }
        break;
    }
    case LOK_CALLBACK_TEXT_SELECTION: {
        m_selection.clear();
        for (const QByteArray &piece : payload.split(';')) {
            QRectF rect;
            if (parseRect(piece, &rect) && !rect.isEmpty()) {
                m_selection << rect;
            }
        }
        if (m_selection.isEmpty()) {
            m_selectionStart = m_selectionEnd = {};
        }
        Q_EMIT selectionChanged();
        break;
    }
    case LOK_CALLBACK_TEXT_SELECTION_START:
        parseRect(payload, &m_selectionStart);
        Q_EMIT selectionChanged();
        break;
    case LOK_CALLBACK_TEXT_SELECTION_END:
        parseRect(payload, &m_selectionEnd);
        Q_EMIT selectionChanged();
        break;
    case LOK_CALLBACK_CELL_CURSOR:
        if (payload.startsWith("EMPTY") || !parseRect(payload, &m_cellCursor)) {
            m_cellCursor = {};
        }
        Q_EMIT cellChanged();
        break;
    case LOK_CALLBACK_CELL_FORMULA:
        m_cellFormula = QString::fromUtf8(payload);
        Q_EMIT cellChanged();
        break;
    case LOK_CALLBACK_CELL_ADDRESS:
        m_cellAddress = QString::fromUtf8(payload);
        Q_EMIT cellChanged();
        break;
    case LOK_CALLBACK_SEARCH_RESULT_SELECTION: {
        // Stepping to one place reports just that place; only a search for
        // every place says how many there are.
        const QJsonObject result = QJsonDocument::fromJson(payload).object();
        if (result.value(QStringLiteral("highlightAll")).toString() != QLatin1String("true")) {
            break;
        }
        m_searchRects.clear();
        const QJsonArray results = result.value(QStringLiteral("searchResultSelection")).toArray();
        for (const QJsonValue &result : results) {
            const QByteArray rects = result.toObject().value(QStringLiteral("rectangles")).toString().toUtf8();
            for (const QByteArray &piece : rects.split(';')) {
                QRectF rect;
                if (parseRect(piece, &rect) && !rect.isEmpty()) {
                    m_searchRects << rect;
                }
            }
        }
        m_searchCount = results.size();
        Q_EMIT searchChanged();
        break;
    }
    case LOK_CALLBACK_SEARCH_NOT_FOUND:
        m_searchRects.clear();
        m_searchCount = 0;
        Q_EMIT searchChanged();
        break;
    case LOK_CALLBACK_STATE_CHANGED: {
        QString name;
        QVariant value;
        if (payload.startsWith('{')) {
            const QJsonObject object = QJsonDocument::fromJson(payload).object();
            name = object.value(QStringLiteral("commandName")).toString();
            value = object.value(QStringLiteral("state")).toVariant();
        } else {
            const int equals = payload.indexOf('=');
            if (equals < 0) {
                break;
            }
            name = QString::fromUtf8(payload.left(equals));
            value = QString::fromUtf8(payload.mid(equals + 1));
        }
        if (name.isEmpty()) {
            break;
        }
        m_states.insert(name, value);
        if (name == QLatin1String(".uno:ModifiedStatus")) {
            const bool modified = value.toString() == QLatin1String("true");
            if (modified != m_modified) {
                m_modified = modified;
                Q_EMIT modifiedChanged();
            }
            if (m_modified) {
                m_autosaveTimer.start();
            }
        }
        Q_EMIT statesChanged();
        break;
    }
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
    case LOK_CALLBACK_SET_PART:
        refreshShape();
        Q_EMIT invalidated({}, m_currentPart);
        break;
    default:
        break;
    }
}

QVariantList OfficeDocument::searchRects() const
{
    QVariantList list;
    for (const QRectF &rect : m_searchRects) {
        list << rect;
    }
    return list;
}

namespace {
// SearchItem.Command: what ExecuteSearch does with what it finds.
enum SearchCommand { FindOne = 0, FindEvery = 1, ReplaceOne = 2, ReplaceEvery = 3 };

QVariantMap searchArguments(const QString &text, SearchCommand what, bool backward = false, const QString &with = {})
{
    return {
        {QStringLiteral("SearchItem.SearchString"), text},
        {QStringLiteral("SearchItem.ReplaceString"), with},
        {QStringLiteral("SearchItem.Backward"), backward},
        {QStringLiteral("SearchItem.Command"), int(what)},
    };
}
}

int OfficeDocument::findAll(const QString &text)
{
    if (text.isEmpty()) {
        clearSearch();
        return 0;
    }
    command(QStringLiteral(".uno:ExecuteSearch"), searchArguments(text, FindEvery));
    settle(200);
    return m_searchCount;
}

void OfficeDocument::findNext(const QString &text, bool backward)
{
    if (!text.isEmpty()) {
        command(QStringLiteral(".uno:ExecuteSearch"), searchArguments(text, FindOne, backward));
    }
}

void OfficeDocument::replace(const QString &text, const QString &with)
{
    if (!text.isEmpty()) {
        command(QStringLiteral(".uno:ExecuteSearch"), searchArguments(text, ReplaceOne, false, with));
    }
}

int OfficeDocument::replaceAll(const QString &text, const QString &with)
{
    const int count = findAll(text);
    if (count > 0) {
        command(QStringLiteral(".uno:ExecuteSearch"), searchArguments(text, ReplaceEvery, false, with));
        settle(200);
    }
    clearSearch();
    return count;
}

void OfficeDocument::clearSearch()
{
    if (m_searchRects.isEmpty() && m_searchCount == 0) {
        return;
    }
    m_searchRects.clear();
    m_searchCount = 0;
    Q_EMIT searchChanged();
}

QStringList OfficeDocument::fontNames()
{
    const QJsonObject values = QJsonDocument::fromJson(commandValues(QStringLiteral(".uno:CharFontName")).toUtf8())
        .object().value(QStringLiteral("commandValues")).toObject();
    QStringList names = values.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

void OfficeDocument::addPageNumbers(const QString &where)
{
    if (m_kind != Text) {
        return;
    }
    // Writing the number moves the cursor into the header or footer; a tap
    // where it was brings it back to the text.
    const QRectF back = m_cursor;
    const bool top = where == QLatin1String("top-corner");
    const QVariantMap on{{QStringLiteral("PageStyle"), QStringLiteral("Default Page Style")}, {QStringLiteral("On"), true}};
    command(top ? QStringLiteral(".uno:InsertPageHeader") : QStringLiteral(".uno:InsertPageFooter"), on);
    command(QStringLiteral(".uno:InsertPageNumberField"));
    command(where == QLatin1String("bottom-middle") ? QStringLiteral(".uno:CenterPara") : QStringLiteral(".uno:RightPara"));
    settle(200);
    if (back.height() > 0) {
        tap(back.x() + 1, back.center().y(), 1);
    }
}

QUrl OfficeDocument::printCopy()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    QDir dir(base + QStringLiteral("/good-input-office"));
    if (!dir.mkpath(QStringLiteral("."))) {
        return {};
    }
    QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    const QUrl target = QUrl::fromLocalFile(dir.filePath(title() + QStringLiteral(".pdf")));
    return saveCopy(target, QStringLiteral("pdf")) ? target : QUrl();
}

QVariantList OfficeDocument::pages() const
{
    QVariantList list;
    for (const QRectF &rect : m_pages) {
        list << rect;
    }
    return list;
}

QVariantList OfficeDocument::selectionRects() const
{
    QVariantList list;
    for (const QRectF &rect : m_selection) {
        list << rect;
    }
    return list;
}

bool OfficeDocument::canUndo() const
{
    return m_states.value(QStringLiteral(".uno:Undo")).toString() != QLatin1String("disabled");
}

bool OfficeDocument::canRedo() const
{
    return m_states.value(QStringLiteral(".uno:Redo")).toString() != QLatin1String("disabled");
}

void OfficeDocument::mouse(int type, qreal x, qreal y, int count, int modifiers)
{
    if (!m_link) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    const int ix = qRound(x);
    const int iy = qRound(y);
    Engine::instance()->run([link, type, ix, iy, count, modifiers] {
        if (link->doc) {
            link->doc->postMouseEvent(type, ix, iy, count, 1, modifiers);
        }
    });
}

void OfficeDocument::tap(qreal x, qreal y, int count)
{
    mouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, x, y, count, 0);
    mouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, x, y, count, 0);
}

void OfficeDocument::press(qreal x, qreal y, int count, int modifiers)
{
    mouse(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, x, y, count, modifiers);
}

void OfficeDocument::move(qreal x, qreal y, int modifiers)
{
    mouse(LOK_MOUSEEVENT_MOUSEMOVE, x, y, 1, modifiers);
}

void OfficeDocument::release(qreal x, qreal y, int count, int modifiers)
{
    mouse(LOK_MOUSEEVENT_MOUSEBUTTONUP, x, y, count, modifiers);
}

void OfficeDocument::moveSelectionEnd(bool end, qreal x, qreal y)
{
    if (!m_link) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    const int ix = qRound(x);
    const int iy = qRound(y);
    Engine::instance()->run([link, end, ix, iy] {
        if (link->doc) {
            link->doc->setTextSelection(end ? LOK_SETTEXTSELECTION_END : LOK_SETTEXTSELECTION_START, ix, iy);
        }
    });
}

void OfficeDocument::clearSelection()
{
    if (!m_link) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->run([link] {
        if (link->doc) {
            link->doc->resetSelection();
        }
    });
}

void OfficeDocument::typeText(const QString &text)
{
    if (!m_link || text.isEmpty()) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->run([link, text] {
        if (!link->doc) {
            return;
        }
        for (const QChar ch : text) {
            const int code = ch == QLatin1Char('\n') ? 13 : ch.unicode();
            const int key = ch == QLatin1Char('\n') ? KeyReturn : 0;
            link->doc->postKeyEvent(LOK_KEYEVENT_KEYINPUT, code, key);
            link->doc->postKeyEvent(LOK_KEYEVENT_KEYUP, code, key);
        }
    });
}

void OfficeDocument::pressKey(int key, int modifiers)
{
    if (!m_link) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    const int code = key | modifiers;
    Engine::instance()->run([link, code] {
        if (link->doc) {
            link->doc->postKeyEvent(LOK_KEYEVENT_KEYINPUT, 0, code);
            link->doc->postKeyEvent(LOK_KEYEVENT_KEYUP, 0, code);
        }
    });
}

void OfficeDocument::command(const QString &uno, const QVariantMap &arguments)
{
    if (!m_link) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    const QByteArray name = uno.toUtf8();
    const QByteArray args = unoArguments(arguments);
    Engine::instance()->run([link, name, args] {
        if (link->doc) {
            link->doc->postUnoCommand(name.constData(), args.isEmpty() ? nullptr : args.constData(), false);
        }
    });
}

QString OfficeDocument::selectedText()
{
    if (!m_link) {
        return {};
    }
    QString text;
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->runAndWait([&] {
        if (link->doc) {
            text = takeString(link->doc->getTextSelection("text/plain;charset=utf-8"));
        }
    });
    return text;
}

QString OfficeDocument::commandValues(const QString &command)
{
    if (!m_link) {
        return {};
    }
    QString text;
    const QByteArray name = command.toUtf8();
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->runAndWait([&] {
        if (link->doc) {
            text = takeString(link->doc->getCommandValues(name.constData()));
        }
    });
    return text;
}

bool OfficeDocument::pasteText(const QString &text)
{
    if (!m_link) {
        return false;
    }
    bool ok = false;
    const QByteArray data = text.toUtf8();
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->runAndWait([&] {
        if (link->doc) {
            ok = link->doc->paste("text/plain;charset=utf-8", data.constData(), data.size());
        }
    });
    return ok;
}

void OfficeDocument::setPart(int part)
{
    if (!m_link || part < 0 || part >= m_parts) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->runAndWait([&] {
        if (link->doc) {
            link->doc->setPart(part);
        }
    });
    refreshShape();
    Q_EMIT invalidated({}, part);
}

bool OfficeDocument::writeTo(const QUrl &target, const QString &format, const QByteArray &options)
{
    if (!m_link || m_status != Ready) {
        return false;
    }
    m_saving = true;
    m_saveError.clear();
    Q_EMIT savingChanged();
    bool ok = false;
    const QByteArray url = target.toString(QUrl::FullyEncoded).toUtf8();
    const QByteArray fmt = format.toUtf8();
    std::shared_ptr<DocumentLink> link = m_link;
    Engine::instance()->runAndWait([&] {
        if (link->doc) {
            ok = link->doc->saveAs(url.constData(), fmt.isEmpty() ? nullptr : fmt.constData(),
                                   options.isEmpty() ? nullptr : options.constData());
        }
    });
    m_saving = false;
    if (!ok) {
        m_saveError = QStringLiteral("Couldn't save to %1").arg(target.toLocalFile());
    }
    Q_EMIT savingChanged();
    return ok;
}

bool OfficeDocument::save()
{
    m_autosaveTimer.stop();
    const bool ok = writeTo(m_source, {}, QByteArrayLiteral("TakeOwnership"));
    if (ok) {
        if (m_modified) {
            m_modified = false;
            Q_EMIT modifiedChanged();
        }
        Q_EMIT saved();
    }
    return ok;
}

bool OfficeDocument::saveCopy(const QUrl &target, const QString &format)
{
    return writeTo(target, format, {});
}

bool OfficeDocument::saveAs(const QUrl &target, const QString &format)
{
    const bool pdf = format == QLatin1String("pdf") || target.path().endsWith(QLatin1String(".pdf"));
    if (pdf) {
        return saveCopy(target, format);
    }
    if (!writeTo(target, format, QByteArrayLiteral("TakeOwnership"))) {
        return false;
    }
    m_source = target;
    Q_EMIT sourceChanged();
    m_modified = false;
    Q_EMIT modifiedChanged();
    Q_EMIT saved();
    return true;
}

void OfficeDocument::settle(int ms)
{
    Engine::instance()->runAndWait([] {});
    if (ms > 0) {
        QThread::msleep(ms);
        Engine::instance()->runAndWait([] {});
    }
    QCoreApplication::processEvents();
}

void OfficeDocument::renderTile(int part, const QRectF &area, const QSize &pixels, QObject *context,
                                std::function<void(QImage)> done)
{
    if (!m_link || pixels.isEmpty() || area.isEmpty()) {
        return;
    }
    std::shared_ptr<DocumentLink> link = m_link;
    const QRect twips = area.toAlignedRect();
    QPointer<QObject> guard(context);
    Engine::instance()->run([link, part, twips, pixels, guard, done = std::move(done)] {
        if (!link->doc) {
            return;
        }
        QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
        // Whatever the engine leaves unpainted stays clear, so the desk shows.
        image.fill(Qt::transparent);
        // The engine writes BGRA bytes, which on a little-endian machine is
        // exactly Qt's premultiplied ARGB32. A part other than the one being
        // edited (a slide's thumbnail) is drawn without moving the editing to it.
        if (link->doc->getParts() > 1 && link->doc->getPart() != part) {
            link->doc->paintPartTile(image.bits(), part, 0, pixels.width(), pixels.height(),
                                     twips.x(), twips.y(), twips.width(), twips.height());
        } else {
            link->doc->paintTile(image.bits(), pixels.width(), pixels.height(),
                                 twips.x(), twips.y(), twips.width(), twips.height());
        }
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, image, done] {
            if (guard) {
                done(image);
            }
        }, Qt::QueuedConnection);
    });
}
