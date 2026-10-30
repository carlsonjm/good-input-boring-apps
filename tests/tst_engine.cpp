#include "Engine.h"
#include "OfficeDocument.h"
#include "Recent.h"

#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>
#include <cstdlib>

// Each test works on its own copy of a fixture, so nothing here changes the
// files in the repository.
class EngineTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QUrl copyOf(const QString &name)
    {
        const QString target = m_dir.filePath(name);
        QFile::remove(target);
        if (!QFile::copy(QStringLiteral(FIXTURES "/") + name, target)) {
            qFatal("could not copy fixture %s", qPrintable(name));
        }
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
        return QUrl::fromLocalFile(target);
    }

    static bool open(OfficeDocument &doc, const QUrl &url)
    {
        doc.setAutosave(false);
        doc.setSource(url);
        return QTest::qWaitFor([&] { return doc.status() == OfficeDocument::Ready
                                            || doc.status() == OfficeDocument::Error; }, 60000)
            && doc.status() == OfficeDocument::Ready;
    }

    // The document as the engine would save it in flat ODF, to read what a
    // setting changed.
    QByteArray flat(OfficeDocument &doc, const QString &name)
    {
        const QString path = m_dir.filePath(name + QStringLiteral(".fodt"));
        if (!doc.saveCopy(QUrl::fromLocalFile(path), QStringLiteral("fodt"))) {
            return {};
        }
        QFile file(path);
        file.open(QIODevice::ReadOnly);
        return file.readAll();
    }

    static QString allText(OfficeDocument &doc)
    {
        doc.selectAll();
        doc.settle(300);
        const QString text = doc.selectedText();
        doc.clearSelection();
        doc.settle();
        return text;
    }

private Q_SLOTS:
    void opensWordFile()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("letter.docx"))));
        QCOMPARE(doc.kind(), OfficeDocument::Text);
        QVERIFY(doc.size().width() > 5000);
        QVERIFY(doc.size().height() > 5000);
        QCOMPARE(doc.title(), QStringLiteral("letter"));
        QVERIFY(allText(doc).contains(QStringLiteral("Garden plan")));
    }

    void refusesAMissingFile()
    {
        OfficeDocument doc;
        doc.setSource(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("nothing-here.docx"))));
        QTRY_VERIFY_WITH_TIMEOUT(doc.status() == OfficeDocument::Error, 30000);
        QVERIFY(!doc.errorString().isEmpty());
    }

    void typesSavesAndReopens()
    {
        const QUrl url = copyOf(QStringLiteral("letter.docx"));
        {
            OfficeDocument doc;
            QVERIFY(open(doc, url));
            doc.command(QStringLiteral(".uno:GoToEndOfDoc"));
            doc.typeText(QStringLiteral(" Typed through Good Input."));
            doc.settle(300);
            QTRY_VERIFY_WITH_TIMEOUT(doc.modified(), 5000);
            QVERIFY(doc.cursorVisible());
            QVERIFY(doc.cursorRect().height() > 0);
            QVERIFY(doc.save());
            QVERIFY(!doc.modified());
        }
        OfficeDocument again;
        QVERIFY(open(again, url));
        QVERIFY(allText(again).contains(QStringLiteral("Typed through Good Input.")));
    }

    void doubleTapSelectsAWord()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("letter.docx"))));
        doc.command(QStringLiteral(".uno:GoToStartOfDoc"));
        doc.settle(300);
        QTRY_VERIFY_WITH_TIMEOUT(doc.cursorRect().height() > 0, 5000);
        const QRectF at = doc.cursorRect();
        // A little into the first word of the heading.
        doc.tap(at.x() + 300, at.center().y(), 2);
        doc.settle(300);
        QTRY_VERIFY_WITH_TIMEOUT(doc.hasSelection(), 5000);
        QCOMPARE(doc.selectedText().trimmed(), QStringLiteral("Garden"));
        QVERIFY(doc.selectionStart().height() > 0);
        QVERIFY(doc.selectionEnd().height() > 0);

        // Dragging the end handle to the end of the line takes the rest of it.
        const QRectF end = doc.selectionEnd();
        doc.moveSelectionEnd(true, end.x() + 6000, end.center().y());
        doc.settle(300);
        QTRY_COMPARE_WITH_TIMEOUT(doc.selectedText().trimmed(), QStringLiteral("Garden plan"), 5000);

        doc.command(QStringLiteral(".uno:Italic"));
        doc.settle(300);
        QTRY_COMPARE_WITH_TIMEOUT(doc.states().value(QStringLiteral(".uno:Italic")).toString(), QStringLiteral("true"), 5000);
    }

    void spreadsheetRecalculates()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("budget.xlsx"))));
        QCOMPARE(doc.kind(), OfficeDocument::Spreadsheet);
        QCOMPARE(doc.parts(), 2);
        QCOMPARE(doc.partNames(), QStringList({QStringLiteral("Budget"), QStringLiteral("Savings")}));
        doc.command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), QStringLiteral("B2")}});
        doc.typeText(QStringLiteral("10\n"));
        doc.command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), QStringLiteral("B4")}});
        doc.settle(300);
        QTRY_COMPARE_WITH_TIMEOUT(doc.cellAddress(), QStringLiteral("B4"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(doc.cellFormula().contains(QStringLiteral("SUM")), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(doc.selectedText().trimmed(), QStringLiteral("14"), 5000);
        QVERIFY(!doc.cellCursor().isEmpty());
    }

    void sheetsTools()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("budget.xlsx"))));
        const auto put = [&](const QString &cell, const QString &text) {
            doc.command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), cell}});
            doc.typeText(text);
            doc.pressKey(1280);
        };
        put(QStringLiteral("C2"), QStringLiteral("5"));
        put(QStringLiteral("C3"), QStringLiteral("7"));
        doc.command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), QStringLiteral("C2:C3")}});
        doc.settle(300);
        QCOMPARE(doc.cellAddress(), QStringLiteral("C2:C3"));
        QTRY_VERIFY2_WITH_TIMEOUT(doc.states().value(QStringLiteral(".uno:StateTableCell")).toString().contains(QStringLiteral("Sum: 12")),
                                  qPrintable(doc.states().value(QStringLiteral(".uno:StateTableCell")).toString()), 3000);
        doc.command(QStringLiteral(".uno:NumberFormatPercent"));
        doc.command(QStringLiteral(".uno:AutoSum"));
        doc.settle(300);
        doc.command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), QStringLiteral("C4")}});
        doc.settle(300);
        QCOMPARE(doc.cellFormula(), QStringLiteral("=SUM(C2:C3)"));
        // The headers the window draws come from the engine.
        const QByteArray headers = doc.commandValues(QStringLiteral(".uno:ViewRowColumnHeaders?x=0&y=0&width=6000&height=3000")).toUtf8();
        QVERIFY(headers.contains("\"text\": \"B\""));
        QVERIFY(headers.contains("\"text\": \"3\""));
    }

    void startsFromATemplate()
    {
        Recent recent;
        recent.setKind(QStringLiteral("spreadsheet"));
        for (const QString &name : {QStringLiteral("budget"), QStringLiteral("tracker"), QStringLiteral("shopping"), QStringLiteral("schedule")}) {
            const QUrl source = recent.templateSource(name);
            QVERIFY2(!source.isEmpty(), qPrintable(name));
            OfficeDocument doc;
            QVERIFY2(open(doc, source), qPrintable(name));
            QCOMPARE(doc.kind(), OfficeDocument::Spreadsheet);
            QCOMPARE(doc.parts(), 1);
        }
        QVERIFY(recent.newDocumentTarget(QStringLiteral("Monthly budget")).fileName().startsWith(QStringLiteral("Monthly budget")));
    }

    void presentationHasSlides()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("deck.pptx"))));
        QCOMPARE(doc.kind(), OfficeDocument::Presentation);
        QCOMPARE(doc.parts(), 3);
        doc.setPart(2);
        QCOMPARE(doc.currentPart(), 2);
    }

    void slidesTools()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("deck.pptx"))));
        // A tap on the title box chooses it.
        doc.tap(doc.size().width() / 2, doc.size().height() * 0.16, 1);
        QTRY_VERIFY_WITH_TIMEOUT(doc.hasObject(), 3000);
        doc.command(QStringLiteral(".uno:DuplicatePage"));
        doc.settle(300);
        QCOMPARE(doc.parts(), 4);
        doc.command(QStringLiteral(".uno:AssignLayout"), {{QStringLiteral("WhatLayout"), 3}});
        doc.command(QStringLiteral(".uno:BasicShapes.round-rectangle"), {{QStringLiteral("KeyModifier"), 8192}});
        doc.settle(300);
        QVERIFY(doc.hasObject());
        const QString path = m_dir.filePath(QStringLiteral("deck.fodp"));
        QVERIFY(doc.saveCopy(QUrl::fromLocalFile(path), QStringLiteral("fodp")));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray saved = file.readAll();
        QVERIFY(saved.contains("presentation:presentation-page-layout-name=\"AL"));
        QVERIFY(saved.contains("draw:type=\"round-rectangle\""));
        doc.command(QStringLiteral(".uno:DeletePage"));
        doc.settle(300);
        QCOMPARE(doc.parts(), 3);
    }

    void drawsAPicture()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("letter.docx"))));
        QImage picture;
        QObject context;
        doc.renderTile(0, QRectF(0, 0, 8000, 4000), QSize(400, 200), &context, [&](QImage image) {
            picture = image;
        });
        QTRY_VERIFY_WITH_TIMEOUT(!picture.isNull(), 10000);
        int dark = 0;
        for (int y = 0; y < picture.height(); y += 2) {
            for (int x = 0; x < picture.width(); x += 2) {
                if (qGray(picture.pixel(x, y)) < 100) {
                    ++dark;
                }
            }
        }
        QVERIFY2(dark > 20, "the page drew no text");
    }

    void findsAndReplaces()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("letter.docx"))));
        const int found = doc.findAll(QStringLiteral("the"));
        QVERIFY(found >= 2);
        QCOMPARE(doc.searchCount(), found);
        QVERIFY(doc.searchRects().size() >= found);
        doc.findNext(QStringLiteral("the"));
        doc.settle(200);
        QCOMPARE(doc.selectedText().toLower(), QStringLiteral("the"));
        // Stepping keeps every place marked.
        QCOMPARE(doc.searchCount(), found);
        QCOMPARE(doc.replaceAll(QStringLiteral("Garden"), QStringLiteral("Orchard")), 1);
        QCOMPARE(doc.searchCount(), 0);
        QVERIFY(allText(doc).contains(QStringLiteral("Orchard plan")));
        QCOMPARE(doc.findAll(QStringLiteral("nowhere in this letter")), 0);
    }

    void setsTheTextAndThePage()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("letter.docx"))));
        QVERIFY(doc.fontNames().contains(QStringLiteral("Liberation Mono")));
        doc.command(QStringLiteral(".uno:GoToStartOfDoc"));
        doc.command(QStringLiteral(".uno:CharFontName"), {{QStringLiteral("CharFontName.FamilyName"), QStringLiteral("Liberation Mono")}});
        doc.command(QStringLiteral(".uno:SpacePara2"));
        doc.command(QStringLiteral(".uno:InsertFootnote"));
        doc.settle(300);
        QCOMPARE(doc.states().value(QStringLiteral(".uno:CharFontName")).toString(), QStringLiteral("Liberation Mono"));
        doc.command(QStringLiteral(".uno:GoToStartOfDoc"));
        // Letter, sideways: 11 by 8.5 inches, in hundredths of a millimetre.
        doc.command(QStringLiteral(".uno:AttributePageSize"), {{QStringLiteral("AttributePageSize.Width"), 27940}, {QStringLiteral("AttributePageSize.Height"), 21590}});
        doc.command(QStringLiteral(".uno:PageLRMargin"), {{QStringLiteral("PageLRMargin.Left"), 1270}, {QStringLiteral("PageLRMargin.Right"), 1270}});
        doc.settle(300);
        doc.addPageNumbers(QStringLiteral("bottom-middle"));
        doc.typeText(QStringLiteral("Back in the text."));
        doc.settle(300);
        const QByteArray saved = flat(doc, QStringLiteral("settings"));
        QVERIFY(saved.contains("Liberation Mono"));
        QVERIFY(saved.contains("fo:line-height=\"200%\""));
        QVERIFY(saved.contains("text:note-class=\"footnote\""));
        QVERIFY(saved.contains("fo:page-width=\"11in\""));
        QVERIFY(saved.contains("fo:page-height=\"8.5in\""));
        QVERIFY(saved.contains("fo:margin-left=\"0.5in\""));
        const int footer = saved.indexOf("<style:footer>");
        QVERIFY(footer > 0);
        QVERIFY(saved.indexOf("<text:page-number", footer) > footer);
        QVERIFY(saved.indexOf("Back in the text.") > saved.indexOf("<office:body>"));
        QVERIFY(QString::fromUtf8(doc.states().value(QStringLiteral(".uno:StateWordCount")).toString().toUtf8()).contains(QStringLiteral("words")));
    }

    void writesAPdfCopy()
    {
        OfficeDocument doc;
        QVERIFY(open(doc, copyOf(QStringLiteral("letter.docx"))));
        const QUrl pdf = QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("letter.pdf")));
        QVERIFY(doc.saveCopy(pdf, QStringLiteral("pdf")));
        QFile file(pdf.toLocalFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(5).startsWith("%PDF"));
        QCOMPARE(doc.source().fileName(), QStringLiteral("letter.docx"));
    }
};

// LibreOffice's own clean-up at exit crashes once its office is gone, so the
// test leaves at once after its results are written, as the program does.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    int code = 0;
    {
        EngineTest test;
        code = QTest::qExec(&test, argc, argv);
    }
    Engine::instance()->shutdown();
    std::fflush(nullptr);
    std::_Exit(code);
}

#include "tst_engine.moc"
