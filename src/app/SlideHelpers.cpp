#include "SlideHelpers.h"

#include "OfficeDocument.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace {

QVariant plain(const QVariant &value);

// One element of a message from the bus: a{sv}, an array, or a plain value.
QVariant element(const QDBusArgument &argument)
{
    switch (argument.currentType()) {
    case QDBusArgument::MapType: {
        QVariantMap map;
        argument.beginMap();
        while (!argument.atEnd()) {
            argument.beginMapEntry();
            const QString key = argument.asVariant().toString();
            map.insert(key, element(argument));
            argument.endMapEntry();
        }
        argument.endMap();
        return map;
    }
    case QDBusArgument::ArrayType: {
        QVariantList list;
        argument.beginArray();
        while (!argument.atEnd()) {
            list.append(element(argument));
        }
        argument.endArray();
        return list;
    }
    case QDBusArgument::VariantType: {
        QDBusVariant variant;
        argument >> variant;
        return plain(variant.variant());
    }
    case QDBusArgument::BasicType:
        return argument.asVariant();
    default:
        return {};
    }
}

// The broker's slides arrive nested inside the bus's own wrapping.
QVariant plain(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>()) {
        return element(value.value<QDBusArgument>());
    }
    if (value.typeId() == QMetaType::QVariantMap) {
        QVariantMap map = value.toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
            it.value() = plain(it.value());
        }
        return map;
    }
    if (value.typeId() == QMetaType::QVariantList) {
        QVariantList list = value.toList();
        for (QVariant &item : list) {
            item = plain(item);
        }
        return list;
    }
    return value;
}

QDBusMessage brokerCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QLatin1String(SlideHelpers::Service), QLatin1String(SlideHelpers::Path),
                                          QLatin1String(SlideHelpers::Interface), method);
}

// The first box of a kind on a slide, by its place in Tab order.
int boxOf(const QVariantList &boxes, const QStringList &kinds)
{
    for (const QString &kind : kinds) {
        for (int i = 0; i < boxes.size(); ++i) {
            if (boxes.at(i).toMap().value(QStringLiteral("kind")).toString() == kind) {
                return i;
            }
        }
    }
    return -1;
}

bool isPlaceholder(const QString &kind)
{
    return kind == QLatin1String("title") || kind == QLatin1String("subtitle") || kind == QLatin1String("outline");
}

} // namespace

SlideHelpers::SlideHelpers(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void SlideHelpers::setDocument(OfficeDocument *document)
{
    if (m_document == document) {
        return;
    }
    m_document = document;
    Q_EMIT documentChanged();
}

int SlideHelpers::layoutFor(const QString &kind)
{
    // The engine's AutoLayout numbers, as Aa offers them. A number is a
    // title slide: the figure large, what it is under it.
    return kind == QLatin1String("text") ? 1 : 0;
}

int SlideHelpers::layoutOf(const QVariantList &boxes)
{
    int titles = 0, subtitles = 0, outlines = 0;
    for (const QVariant &box : boxes) {
        const QString kind = box.toMap().value(QStringLiteral("kind")).toString();
        titles += kind == QLatin1String("title");
        subtitles += kind == QLatin1String("subtitle");
        outlines += kind == QLatin1String("outline");
    }
    if (subtitles > 0) {
        return 0;
    }
    if (titles > 0) {
        return outlines >= 2 ? 3 : outlines == 1 ? 1 : 19;
    }
    return outlines > 0 ? 32 : -1;
}

void SlideHelpers::refresh()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const bool was = m_available;
    const bool wasReady = m_codexReady;
    m_available = bus.isConnected() && bus.interface()
        && (bus.interface()->isServiceRegistered(QLatin1String(Service))
            || bus.interface()->activatableServiceNames().value().contains(QLatin1String(Service)));
    if (m_available && !m_listening) {
        m_listening = bus.connect(QLatin1String(Service), QLatin1String(Path), QLatin1String(Interface), QStringLiteral("Answered"),
                                  this, SLOT(answered(uint, QVariantMap)))
            && bus.connect(QLatin1String(Service), QLatin1String(Path), QLatin1String(Interface), QStringLiteral("Failed"),
                           this, SLOT(failed(uint, QString)));
    }
    if (!m_available) {
        m_codexReady = false;
    } else {
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(brokerCall(QStringLiteral("Codex"))), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            watcher->deleteLater();
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            const bool ready = !reply.isError() && reply.value().value(QStringLiteral("signedIn")).toBool();
            if (ready != m_codexReady) {
                m_codexReady = ready;
                Q_EMIT stateChanged();
            }
        });
    }
    if (was != m_available || wasReady != m_codexReady) {
        Q_EMIT stateChanged();
    }
}

void SlideHelpers::ask(const QString &method, const QVariant &argument, const QString &kind)
{
    QDBusMessage call = brokerCall(method);
    call.setArguments({argument});
    m_working = kind;
    m_problem.clear();
    m_result.clear();
    m_request = 0;
    Q_EMIT stateChanged();
    // Codex takes its time; the reply carries only the request's number.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, kind](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<uint> reply = *watcher;
        if (m_working != kind) {
            return;
        }
        if (reply.isError()) {
            finish({}, tr("Split Rock's helpers couldn't be reached."));
            return;
        }
        m_request = reply.value();
    });
}

void SlideHelpers::fit()
{
    if (!m_document || !m_working.isEmpty()) {
        return;
    }
    const QVariantList slides = m_document->slideBoxes();
    m_part = m_document->currentPart();
    const QVariantList boxes = slides.value(m_part).toList();
    const int title = boxOf(boxes, {QStringLiteral("title")});
    const int body = boxOf(boxes, {QStringLiteral("outline"), QStringLiteral("text"), QStringLiteral("subtitle")});
    if (body < 0) {
        m_problem = tr("This slide has no text to fit.");
        Q_EMIT stateChanged();
        return;
    }
    QStringList others;
    for (int i = 0; i < slides.size(); ++i) {
        const QVariantList theirs = slides.at(i).toList();
        const int theirTitle = boxOf(theirs, {QStringLiteral("title")});
        if (i != m_part && theirTitle >= 0) {
            others.append(theirs.at(theirTitle).toMap().value(QStringLiteral("text")).toString());
        }
    }
    const QVariantMap slide{
        {QStringLiteral("title"), title >= 0 ? boxes.at(title).toMap().value(QStringLiteral("text")).toString() : QString()},
        {QStringLiteral("lines"), boxes.at(body).toMap().value(QStringLiteral("text")).toString().split(QLatin1Char('\n'))},
        {QStringLiteral("others"), others}};
    ask(QStringLiteral("FitSlide"), slide, QStringLiteral("fit"));
}

void SlideHelpers::make(const QString &notes)
{
    if (!m_document || !m_working.isEmpty() || notes.trimmed().isEmpty()) {
        return;
    }
    m_part = m_document->currentPart();
    ask(QStringLiteral("MakeSlides"), notes.trimmed(), QStringLiteral("make"));
}

void SlideHelpers::tidy()
{
    if (!m_document || !m_working.isEmpty()) {
        return;
    }
    m_working = QStringLiteral("tidy");
    m_problem.clear();
    m_result.clear();
    Q_EMIT stateChanged();
    m_part = m_document->currentPart();
    m_mark = m_document->undoDepth();
    // The layout again puts its boxes back where it keeps them; then each
    // box loses what was set on it by hand and takes the deck's look.
    const int layout = layoutOf(m_document->slideBoxes().value(m_part).toList());
    if (layout >= 0) {
        m_document->command(QStringLiteral(".uno:AssignLayout"), {{QStringLiteral("WhatLayout"), layout}});
        m_document->settle(200);
    }
    const QVariantList boxes = m_document->slideBoxes().value(m_part).toList();
    for (int i = 0; i < boxes.size(); ++i) {
        if (isPlaceholder(boxes.at(i).toMap().value(QStringLiteral("kind")).toString())) {
            m_document->plainBox(m_part, i);
        }
    }
    finish({{QStringLiteral("line"), tr("The boxes are back in their places, with the deck's own look.")},
            {QStringLiteral("who"), QStringLiteral("app")}});
}

void SlideHelpers::answered(uint id, const QVariantMap &result)
{
    if (id == 0 || id != m_request) {
        return;
    }
    const QVariantMap answer = plain(result).toMap();
    apply(m_working, answer.value(QStringLiteral("slides")).toList(), answer.value(QStringLiteral("line")).toString());
}

void SlideHelpers::failed(uint id, const QString &problem)
{
    if (id == 0 || id != m_request) {
        return;
    }
    finish({}, problem == QLatin1String("cancelled") ? QString() : problem);
}

void SlideHelpers::apply(const QString &kind, const QVariantList &slides, const QString &line)
{
    if (!m_document || slides.isEmpty()) {
        finish({}, tr("Codex's slides didn't fit."));
        return;
    }
    // Codex's answer goes on the slide it was asked about, wherever the
    // person has gone since.
    if (m_working.isEmpty()) {
        m_part = m_document->currentPart();
    }
    m_working = QStringLiteral("apply");
    Q_EMIT stateChanged();
    m_mark = m_document->undoDepth();
    int at = m_part;
    if (kind == QLatin1String("fit")) {
        fill(m_part, slides.first().toMap());
    } else {
        // A new deck's one empty slide becomes the first; otherwise the new
        // slides follow the one shown.
        const QVariantList deck = m_document->slideBoxes();
        bool empty = deck.size() == 1;
        for (const QVariant &box : deck.value(0).toList()) {
            empty = empty && box.toMap().value(QStringLiteral("text")).toString().isEmpty();
        }
        if (empty) {
            m_document->command(QStringLiteral(".uno:AssignLayout"),
                                {{QStringLiteral("WhatLayout"), layoutFor(slides.first().toMap().value(QStringLiteral("layout")).toString())}});
            m_document->settle(200);
            fill(0, slides.first().toMap());
            at = 0;
        } else {
            addSlide(at, slides.first().toMap());
            ++at;
        }
    }
    for (int i = 1; i < slides.size(); ++i) {
        addSlide(at, slides.at(i).toMap());
        ++at;
    }
    m_document->setPart(kind == QLatin1String("fit") ? m_part : at - slides.size() + 1);
    finish({{QStringLiteral("line"), line},
            {QStringLiteral("who"), QStringLiteral("codex")},
            {QStringLiteral("many"), slides.size() > 1 && kind == QLatin1String("make")}});
}

void SlideHelpers::addSlide(int after, const QVariantMap &slide)
{
    const int before = m_document->parts();
    m_document->setPart(after);
    m_document->settle(100);
    m_document->command(QStringLiteral(".uno:InsertPage"));
    QElapsedTimer waited;
    waited.start();
    while (m_document->parts() == before && waited.elapsed() < 5000) {
        m_document->settle(100);
    }
    m_document->setPart(after + 1);
    m_document->settle(100);
    m_document->command(QStringLiteral(".uno:AssignLayout"),
                        {{QStringLiteral("WhatLayout"), layoutFor(slide.value(QStringLiteral("layout")).toString())}});
    m_document->settle(200);
    fill(after + 1, slide);
}

// The title in the slide's title box, the lines in its text or under the
// title on a title slide.
void SlideHelpers::fill(int part, const QVariantMap &slide)
{
    const QVariantList boxes = m_document->slideBoxes().value(part).toList();
    const int title = boxOf(boxes, {QStringLiteral("title")});
    const int body = boxOf(boxes, {QStringLiteral("outline"), QStringLiteral("subtitle"), QStringLiteral("text")});
    if (title >= 0) {
        m_document->setBoxText(part, title, slide.value(QStringLiteral("title")).toString());
    }
    const QStringList lines = slide.value(QStringLiteral("lines")).toStringList();
    if (body >= 0 && !lines.isEmpty()) {
        m_document->setBoxText(part, body, lines.join(QLatin1Char('\n')));
    }
}

void SlideHelpers::finish(const QVariantMap &result, const QString &problem)
{
    m_working.clear();
    m_request = 0;
    m_result = result;
    m_problem = problem;
    Q_EMIT stateChanged();
}

void SlideHelpers::keep()
{
    m_mark = -1;
    m_result.clear();
    Q_EMIT stateChanged();
}

void SlideHelpers::undo()
{
    if (m_document && m_mark >= 0) {
        m_document->undoTo(m_mark);
        m_document->setPart(qMin(m_part, m_document->parts() - 1));
    }
    keep();
}

void SlideHelpers::cancel()
{
    if (m_request != 0) {
        QDBusMessage call = brokerCall(QStringLiteral("Cancel"));
        call.setArguments({m_request});
        QDBusConnection::sessionBus().asyncCall(call);
    }
    finish({});
}

void SlideHelpers::dismiss()
{
    m_problem.clear();
    Q_EMIT stateChanged();
}

void SlideHelpers::signIn()
{
    // Ask opens Codex's own window; with no question it only signs in.
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("io.github.carlsonjm.splitrock.Ask"), QStringLiteral("/Ask"),
                                                       QStringLiteral("io.github.carlsonjm.SplitRock.Ask"), QStringLiteral("OpenCodex"));
    call.setArguments({QString()});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        // Signing in happens in Codex's window; look again a little later.
        QTimer::singleShot(15000, this, &SlideHelpers::refresh);
    });
}

QString SlideHelpers::wordsOf(const QUrl &file)
{
    const QString path = file.toLocalFile();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("txt") || suffix == QLatin1String("md")) {
        QFile text(path);
        return text.open(QIODevice::ReadOnly) ? QString::fromUtf8(text.read(12000)) : QString();
    }
    OfficeDocument document;
    document.setAutosave(false);
    document.setSource(file);
    QEventLoop wait;
    QTimer limit;
    limit.setSingleShot(true);
    connect(&limit, &QTimer::timeout, &wait, &QEventLoop::quit);
    connect(&document, &OfficeDocument::statusChanged, &wait, [&] {
        if (document.status() == OfficeDocument::Ready || document.status() == OfficeDocument::Error) {
            wait.quit();
        }
    });
    limit.start(30000);
    if (document.status() != OfficeDocument::Ready) {
        wait.exec();
    }
    if (document.status() != OfficeDocument::Ready) {
        return {};
    }
    document.selectAll();
    document.settle(200);
    return document.selectedText().left(12000);
}
