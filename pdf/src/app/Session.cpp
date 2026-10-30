#include "Session.h"

#include "Saver.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMutexLocker>

#include <algorithm>

using namespace pdf;

static QVariantMap rectMap(const QRectF &r)
{
    return {{QStringLiteral("x"), r.x()}, {QStringLiteral("y"), r.y()},
            {QStringLiteral("w"), r.width()}, {QStringLiteral("h"), r.height()}};
}

static QVariantMap runMap(const TextRun &run)
{
    QVariantList rects;
    for (const QRectF &r : run.rects)
        rects.append(rectMap(r));
    return {{QStringLiteral("text"), run.text}, {QStringLiteral("rects"), rects}};
}

static QString saveWord(Saver::Result result)
{
    switch (result) {
    case Saver::Result::Saved:
        return QStringLiteral("saved");
    case Saver::Result::NothingOpen:
        return QStringLiteral("nothingOpen");
    case Saver::Result::CannotWrite:
        return QStringLiteral("cannotWrite");
    case Saver::Result::Failed:
        break;
    }
    return QStringLiteral("failed");
}

Session::Session(const QString &dataDir, const QString &settingsFile, QObject *parent)
    : QObject(parent)
    , m_signature(dataDir)
    , m_places(settingsFile)
{
}

Session::~Session()
{
    close();
}

QString Session::title() const { QMutexLocker l(&m_lock); return m_file.title(); }
QUrl Session::url() const { QMutexLocker l(&m_lock); return m_file.isOpen() ? QUrl::fromLocalFile(m_file.path()) : QUrl(); }
int Session::pageCount() const { QMutexLocker l(&m_lock); return m_file.pageCount(); }
bool Session::changed() const { return m_changed; }
int Session::fieldCount() const { return m_form ? m_form->count() : 0; }
int Session::stillNeeded() const { return m_form ? m_form->stillNeeded() : 0; }
bool Session::hasSignature() const { return m_signature.has(); }

bool Session::open(const QUrl &url, const QString &password)
{
    close();
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    PdfFile::OpenResult result;
    {
        QMutexLocker l(&m_lock);
        result = m_file.open(path, password);
    }
    switch (result) {
    case PdfFile::OpenResult::Opened:
        m_status = QStringLiteral("open");
        break;
    case PdfFile::OpenResult::Missing:
        m_status = QStringLiteral("missing");
        break;
    case PdfFile::OpenResult::NotPdf:
        m_status = QStringLiteral("notPdf");
        break;
    case PdfFile::OpenResult::NeedsPassword:
        m_status = QStringLiteral("needsPassword");
        break;
    case PdfFile::OpenResult::WrongPassword:
        m_status = QStringLiteral("wrongPassword");
        break;
    }
    if (result == PdfFile::OpenResult::Opened) {
        m_marks = std::make_unique<Marks>(m_file);
        reloadForm();
        m_page = std::clamp(m_places.page(m_file.path()), 0, m_file.pageCount() - 1);
    }
    Q_EMIT opened();
    Q_EMIT pageChanged();
    touched(true);
    return result == PdfFile::OpenResult::Opened;
}

void Session::close()
{
    if (m_file.isOpen())
        m_places.remember(m_file.path(), m_page);
    QMutexLocker l(&m_lock);
    m_form.reset();
    m_marks.reset();
    m_file.close();
    m_status = QStringLiteral("empty");
    m_page = 0;
    m_changed = false;
    m_lastQuery.clear();
    m_searchHits.clear();
}

void Session::reloadForm()
{
    QMutexLocker l(&m_lock);
    m_form = std::make_unique<FormFields>(m_file);
}

void Session::touched(bool formToo)
{
    ++m_revision;
    Q_EMIT revisionChanged();
    Q_EMIT changedChanged();
    if (formToo)
        Q_EMIT formChanged();
}

void Session::setPage(int page)
{
    page = std::clamp(page, 0, std::max(0, pageCount() - 1));
    if (page == m_page)
        return;
    m_page = page;
    Q_EMIT pageChanged();
}

double Session::pageAspect(int page) const
{
    QMutexLocker l(&m_lock);
    const QSizeF size = m_file.pageSize(page);
    return size.isEmpty() ? 11.0 / 8.5 : size.height() / size.width();
}

QImage Session::renderPage(int page, int widthPx) const
{
    QMutexLocker l(&m_lock);
    return m_file.render(page, widthPx);
}

QVariantMap Session::wordAt(int page, double x, double y) const
{
    QMutexLocker l(&m_lock);
    return runMap(m_file.wordAt(page, QPointF(x, y)));
}

QVariantMap Session::textBetween(int page, double x1, double y1, double x2, double y2) const
{
    QMutexLocker l(&m_lock);
    return runMap(m_file.textBetween(page, QPointF(x1, y1), QPointF(x2, y2)));
}

void Session::copy(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard(); clipboard && !text.isEmpty())
        clipboard->setText(text);
}

QVariantList Session::highlightColours() const
{
    QVariantList out;
    for (const QColor &c : Marks::highlightColours())
        out.append(c);
    return out;
}

QString Session::highlight(int page, const QVariantList &rects, int colour)
{
    const auto colours = Marks::highlightColours();
    if (!m_marks || colour < 0 || colour >= colours.size())
        return {};
    QList<QRectF> list;
    for (const QVariant &v : rects) {
        const QVariantMap m = v.toMap();
        list.append(QRectF(m.value(QStringLiteral("x")).toDouble(), m.value(QStringLiteral("y")).toDouble(),
                           m.value(QStringLiteral("w")).toDouble(), m.value(QStringLiteral("h")).toDouble()));
    }
    QString name;
    {
        QMutexLocker l(&m_lock);
        name = m_marks->addHighlight(page, list, colours.at(colour));
    }
    if (!name.isEmpty()) {
        m_changed = true;
        touched(false);
    }
    return name;
}

QVariantList Session::highlights(int page) const
{
    QVariantList out;
    if (!m_marks)
        return out;
    QMutexLocker l(&m_lock);
    for (const auto &h : m_marks->highlights(page)) {
        QVariantList rects;
        for (const QRectF &r : h.rects)
            rects.append(rectMap(r));
        out.append(QVariantMap{{QStringLiteral("name"), h.name}, {QStringLiteral("rects"), rects},
                               {QStringLiteral("colour"), h.color},
                               {QStringLiteral("ours"), h.name.startsWith(Marks::namePrefix())}});
    }
    return out;
}

bool Session::removeMark(int page, const QString &name)
{
    if (!m_marks)
        return false;
    bool removed;
    {
        QMutexLocker l(&m_lock);
        removed = m_marks->remove(page, name);
    }
    if (removed) {
        m_changed = true;
        touched(false);
    }
    return removed;
}

void Session::search(const QString &query)
{
    if (query == m_lastQuery)
        return;
    m_lastQuery = query;
    m_searchHits.clear();
    QMutexLocker l(&m_lock);
    for (const TextRun &hit : m_file.search(query)) {
        QVariantMap m = rectMap(hit.rects.first());
        m.insert(QStringLiteral("page"), hit.page);
        m_searchHits.append(m);
    }
    l.unlock();
    Q_EMIT searchHitsChanged();
}

QVariantList Session::pagesWithHits() const
{
    QVariantList out;
    int lastPage = -1;
    int count = 0;
    for (const QVariant &v : m_searchHits) {
        const int page = v.toMap().value(QStringLiteral("page")).toInt();
        if (page != lastPage && lastPage >= 0) {
            out.append(QVariantMap{{QStringLiteral("page"), lastPage}, {QStringLiteral("count"), count}});
            count = 0;
        }
        lastPage = page;
        ++count;
    }
    if (lastPage >= 0)
        out.append(QVariantMap{{QStringLiteral("page"), lastPage}, {QStringLiteral("count"), count}});
    return out;
}

static QString kindWord(FormFields::Kind kind)
{
    switch (kind) {
    case FormFields::Kind::Text:
        return QStringLiteral("text");
    case FormFields::Kind::Check:
        return QStringLiteral("check");
    case FormFields::Kind::Radio:
        return QStringLiteral("radio");
    case FormFields::Kind::Choice:
        return QStringLiteral("choice");
    case FormFields::Kind::Signature:
        return QStringLiteral("signature");
    case FormFields::Kind::Other:
        break;
    }
    return QStringLiteral("other");
}

QVariantMap Session::field(int index) const
{
    if (!m_form || index < 0 || index >= m_form->count())
        return {};
    const auto &f = m_form->at(index);
    QVariantMap m = rectMap(f.rect);
    m.insert(QStringLiteral("index"), index);
    m.insert(QStringLiteral("page"), f.page);
    m.insert(QStringLiteral("kind"), kindWord(f.kind));
    m.insert(QStringLiteral("label"), f.label);
    m.insert(QStringLiteral("readOnly"), f.readOnly);
    m.insert(QStringLiteral("multiLine"), f.multiLine);
    m.insert(QStringLiteral("text"), f.text);
    m.insert(QStringLiteral("checked"), f.checked);
    m.insert(QStringLiteral("options"), f.options);
    m.insert(QStringLiteral("empty"), m_form->isMissing(index));
    return m;
}

QVariantList Session::fieldsOnPage(int page) const
{
    QVariantList out;
    for (int i = 0; m_form && i < m_form->count(); ++i)
        if (m_form->at(i).page == page)
            out.append(field(i));
    return out;
}

bool Session::setFieldText(int index, const QString &text)
{
    if (!m_form)
        return false;
    bool ok;
    {
        QMutexLocker l(&m_lock);
        ok = m_form->setText(index, text);
    }
    if (ok && m_form->changed()) {
        m_changed = true;
        touched(true);
    }
    return ok;
}

bool Session::setFieldChecked(int index, bool checked)
{
    if (!m_form)
        return false;
    bool ok;
    {
        QMutexLocker l(&m_lock);
        ok = m_form->setChecked(index, checked);
    }
    if (ok && m_form->changed()) {
        m_changed = true;
        touched(true);
    }
    return ok;
}

bool Session::setFieldChoice(int index, const QString &option)
{
    if (!m_form)
        return false;
    bool ok;
    {
        QMutexLocker l(&m_lock);
        ok = m_form->setChoice(index, option);
    }
    if (ok) {
        m_changed = true;
        touched(true);
    }
    return ok;
}

int Session::nextField(int index) const { return m_form ? m_form->next(index) : -1; }
int Session::previousField(int index) const { return m_form ? m_form->previous(index) : -1; }
int Session::firstEmptyField() const { return m_form ? m_form->firstMissing() : -1; }

bool Session::saveSignature(const QVariantList &strokes, double aspect)
{
    SignatureInk ink;
    ink.aspect = aspect > 0 ? aspect : ink.aspect;
    for (const QVariant &s : strokes) {
        QList<QPointF> stroke;
        for (const QVariant &p : s.toList()) {
            const QVariantMap m = p.toMap();
            stroke.append(QPointF(m.value(QStringLiteral("x")).toDouble(), m.value(QStringLiteral("y")).toDouble()));
        }
        ink.strokes.append(stroke);
    }
    const bool ok = m_signature.save(ink);
    if (ok)
        Q_EMIT signatureChanged();
    return ok;
}

QVariantList Session::signatureStrokes() const
{
    QVariantList out;
    for (const auto &stroke : m_signature.load().strokes) {
        QVariantList points;
        for (const QPointF &p : stroke)
            points.append(QVariantMap{{QStringLiteral("x"), p.x()}, {QStringLiteral("y"), p.y()}});
        out.append(QVariant(points));
    }
    return out;
}

double Session::signatureAspect() const { return m_signature.load().aspect; }

bool Session::forgetSignature()
{
    const bool ok = m_signature.forget();
    Q_EMIT signatureChanged();
    return ok;
}

QString Session::placeSignature(int page, double x, double y, double w, double h)
{
    if (!m_marks)
        return {};
    const SignatureInk ink = m_signature.load();
    QString name;
    {
        QMutexLocker l(&m_lock);
        name = m_marks->placeSignature(page, QRectF(x, y, w, h), ink);
    }
    if (!name.isEmpty()) {
        m_changed = true;
        touched(false);
    }
    return name;
}

QString Session::save()
{
    Saver::Result result;
    {
        QMutexLocker l(&m_lock);
        result = Saver::save(m_file);
        if (result == Saver::Result::Saved) {
            // The saved file was opened again; marks and fields follow it.
            m_marks = std::make_unique<Marks>(m_file);
            m_form = std::make_unique<FormFields>(m_file);
        }
    }
    if (result == Saver::Result::Saved) {
        m_changed = false;
        touched(true);
    }
    return saveWord(result);
}

QString Session::saveCopy(const QUrl &target)
{
    QMutexLocker l(&m_lock);
    return saveWord(Saver::saveCopy(m_file, target.toLocalFile()));
}
