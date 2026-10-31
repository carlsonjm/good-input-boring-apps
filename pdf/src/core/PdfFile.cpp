#include "PdfFile.h"

#include <poppler-qt6.h>

#include <QFileInfo>
#include <QPainter>

#include <algorithm>

namespace pdf {

PdfFile::PdfFile() = default;
PdfFile::~PdfFile() = default;

PdfFile::OpenResult PdfFile::open(const QString &path, const QString &password)
{
    close();
    if (!QFileInfo(path).isFile())
        return OpenResult::Missing;

    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
    if (!doc)
        return OpenResult::NotPdf;
    if (doc->isLocked()) {
        if (password.isEmpty())
            return OpenResult::NeedsPassword;
        const QByteArray secret = password.toUtf8();
        if (doc->unlock(secret, secret) || doc->isLocked())
            return OpenResult::WrongPassword;
    }
    if (doc->numPages() < 1)
        return OpenResult::NotPdf;

    doc->setRenderHint(Poppler::Document::Antialiasing);
    doc->setRenderHint(Poppler::Document::TextAntialiasing);
    doc->setRenderHint(Poppler::Document::TextHinting);
    m_document = std::move(doc);
    m_path = QFileInfo(path).absoluteFilePath();
    return OpenResult::Opened;
}

void PdfFile::close()
{
    m_document.reset();
    m_path.clear();
}

bool PdfFile::isOpen() const { return m_document != nullptr; }
QString PdfFile::path() const { return m_path; }
Poppler::Document *PdfFile::document() const { return m_document.get(); }

QString PdfFile::title() const
{
    if (!m_document)
        return {};
    const QString own = m_document->info(QStringLiteral("Title")).trimmed();
    return own.isEmpty() ? QFileInfo(m_path).completeBaseName() : own;
}

int PdfFile::pageCount() const { return m_document ? m_document->numPages() : 0; }

std::unique_ptr<Poppler::Page> PdfFile::page(int index) const
{
    if (!m_document || index < 0 || index >= m_document->numPages())
        return nullptr;
    return m_document->page(index);
}

QSizeF PdfFile::pageSize(int index) const
{
    const auto p = page(index);
    return p ? p->pageSizeF() : QSizeF();
}

QImage PdfFile::render(int index, int widthPx) const
{
    const auto p = page(index);
    if (!p || widthPx <= 0)
        return {};
    const QSizeF points = p->pageSizeF();
    if (points.width() <= 0)
        return {};
    const double dpi = 72.0 * widthPx / points.width();
    QImage image = p->renderToImage(dpi, dpi);
    if (image.isNull())
        return {};
    // Poppler may leave the paper transparent; a page is always white paper.
    if (image.hasAlphaChannel()) {
        QImage paper(image.size(), QImage::Format_ARGB32_Premultiplied);
        paper.fill(Qt::white);
        QPainter painter(&paper);
        painter.drawImage(0, 0, image);
        painter.end();
        return paper;
    }
    return image;
}

QList<PdfFile::Word> PdfFile::words(int index) const
{
    QList<Word> out;
    const auto p = page(index);
    if (!p)
        return out;
    const QSizeF size = p->pageSizeF();
    if (size.isEmpty())
        return out;
    const auto boxes = p->textList();
    out.reserve(boxes.size());
    for (const auto &box : boxes) {
        const QRectF r = box->boundingBox();
        out.append({box->text(),
                    QRectF(r.x() / size.width(), r.y() / size.height(),
                           r.width() / size.width(), r.height() / size.height()),
                    box->hasSpaceAfter()});
    }
    return out;
}

QString PdfFile::pageText(int index) const
{
    QString text;
    const auto list = words(index);
    for (int i = 0; i < list.size(); ++i) {
        text += list[i].text;
        if (list[i].spaceAfter)
            text += QLatin1Char(' ');
        else if (i + 1 < list.size() && list[i + 1].rect.top() > list[i].rect.bottom())
            text += QLatin1Char('\n');
    }
    return text;
}

QList<TextRun> PdfFile::search(const QString &query) const
{
    QList<TextRun> hits;
    const QString needle = query.trimmed();
    if (!m_document || needle.isEmpty())
        return hits;
    const auto flags = Poppler::Page::IgnoreCase | Poppler::Page::IgnoreDiacritics;
    for (int i = 0; i < pageCount(); ++i) {
        const auto p = page(i);
        const QSizeF size = p->pageSizeF();
        for (const QRectF &r : p->search(needle, flags)) {
            hits.append({i, needle,
                         {QRectF(r.x() / size.width(), r.y() / size.height(),
                                 r.width() / size.width(), r.height() / size.height())}});
        }
    }
    return hits;
}

static double distance(const QRectF &r, const QPointF &p)
{
    const double dx = std::max({r.left() - p.x(), 0.0, p.x() - r.right()});
    const double dy = std::max({r.top() - p.y(), 0.0, p.y() - r.bottom()});
    return dx * dx + dy * dy;
}

static int nearestWord(const QList<QRectF> &rects, const QPointF &point)
{
    int best = -1;
    double bestDistance = 0;
    for (int i = 0; i < rects.size(); ++i) {
        const double d = distance(rects[i], point);
        if (best < 0 || d < bestDistance) {
            best = i;
            bestDistance = d;
        }
    }
    return best;
}

TextRun PdfFile::wordAt(int index, const QPointF &point) const
{
    const auto list = words(index);
    for (const Word &w : list) {
        // A little slack, so a finger landing in the gap between lines still finds a word.
        if (w.rect.adjusted(-0.004, -0.004, 0.004, 0.004).contains(point))
            return {index, w.text, {w.rect}};
    }
    return {};
}

TextRun PdfFile::textBetween(int index, const QPointF &from, const QPointF &to) const
{
    const auto list = words(index);
    if (list.isEmpty())
        return {};
    QList<QRectF> rects;
    rects.reserve(list.size());
    for (const Word &w : list)
        rects.append(w.rect);
    int a = nearestWord(rects, from);
    int b = nearestWord(rects, to);
    if (a > b)
        std::swap(a, b);

    TextRun run{index, {}, {}};
    for (int i = a; i <= b; ++i) {
        run.text += list[i].text;
        if (i < b)
            run.text += list[i].spaceAfter ? QStringLiteral(" ") : QStringLiteral("\n");
        // One rectangle per line, so a highlight reads as a band rather than boxes.
        if (!run.rects.isEmpty() && qAbs(run.rects.last().center().y() - list[i].rect.center().y())
                                        < list[i].rect.height() / 2)
            run.rects.last() = run.rects.last().united(list[i].rect);
        else
            run.rects.append(list[i].rect);
    }
    return run;
}

} // namespace pdf
