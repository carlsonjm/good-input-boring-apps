#pragma once

#include <QImage>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <memory>

namespace Poppler {
class Document;
class Page;
}

namespace pdf {

// A run of text on one page and where it sits. Rectangles are normalised to
// the page, 0 to 1 from the top-left corner, so the look can draw them at any
// zoom without knowing the page's size in points.
struct TextRun {
    int page = -1;
    QString text;
    QList<QRectF> rects;

    bool isEmpty() const { return text.isEmpty(); }
};

// One open PDF. It knows nothing of the screen: it opens, measures, draws a
// page to a picture, finds text and hands out the pages for forms and marks.
class PdfFile
{
public:
    enum class OpenResult {
        Opened,
        Missing,       // no file at that path
        NotPdf,        // the file is there but is not a PDF we can read
        NeedsPassword, // locked; ask for the password
        WrongPassword,
    };

    PdfFile();
    ~PdfFile();
    PdfFile(const PdfFile &) = delete;
    PdfFile &operator=(const PdfFile &) = delete;

    OpenResult open(const QString &path, const QString &password = {});
    void close();

    bool isOpen() const;
    QString path() const;
    // The document's own title, or the file name without ".pdf" when it has none.
    QString title() const;
    int pageCount() const;
    // In points, 1/72 inch, already turned for a page that is rotated.
    QSizeF pageSize(int page) const;

    // Draws a page `widthPx` wide, keeping its proportions, on white paper.
    QImage render(int page, int widthPx) const;

    // Every place `query` appears, in page order, ignoring case and accents.
    QList<TextRun> search(const QString &query) const;
    // The word under a point, for a double tap.
    TextRun wordAt(int page, const QPointF &point) const;
    // The text between two points in reading order, for a drag of the
    // selection's ends; works whichever way round the ends are.
    TextRun textBetween(int page, const QPointF &from, const QPointF &to) const;

    // The whole text of a page, in reading order.
    QString pageText(int page) const;

    Poppler::Document *document() const;
    std::unique_ptr<Poppler::Page> page(int index) const;

private:
    struct Word {
        QString text;
        QRectF rect;   // normalised
        bool spaceAfter = false;
    };
    QList<Word> words(int page) const;

    std::unique_ptr<Poppler::Document> m_document;
    QString m_path;
};

} // namespace pdf
