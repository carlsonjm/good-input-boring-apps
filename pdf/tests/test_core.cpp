#include "FormFields.h"
#include "Marks.h"
#include "PdfFile.h"
#include "ReadingPlace.h"
#include "Saver.h"
#include "Signature.h"

#include <poppler-annotation.h>
#include <poppler-qt6.h>

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace pdf;

static constexpr auto kAnyone = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
                                | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther
                                | QFile::WriteOther | QFile::ExeOther;

class CoreTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString copyOf(const QString &name)
    {
        const QString target = m_dir.filePath(QStringLiteral("%1-%2").arg(++m_copies).arg(name));
        QFile::copy(QStringLiteral(FIXTURES "/") + name, target);
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
        return target;
    }
    int m_copies = 0;

private Q_SLOTS:
    void opensAndMeasures()
    {
        PdfFile file;
        QCOMPARE(file.open(QStringLiteral(FIXTURES "/lease.pdf")), PdfFile::OpenResult::Opened);
        QCOMPARE(file.pageCount(), 3);
        QCOMPARE(file.title(), QStringLiteral("Lease agreement"));
        QCOMPARE(file.pageSize(0), QSizeF(612, 792));
    }

    void refusesWhatIsNotAPdf()
    {
        PdfFile file;
        QCOMPARE(file.open(QStringLiteral(FIXTURES "/missing.pdf")), PdfFile::OpenResult::Missing);
        QCOMPARE(file.open(QStringLiteral(FIXTURES "/not-a-pdf.pdf")), PdfFile::OpenResult::NotPdf);
        QVERIFY(!file.isOpen());
    }

    void titleFallsBackToFileName()
    {
        // form.pdf has a title; a copy with its Info removed must use its name.
        const QString path = copyOf(QStringLiteral("lease.pdf"));
        QByteArray bytes;
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            bytes = f.readAll();
        }
        bytes.replace("/Title (Lease agreement)", "/Title ()               ");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        }
        PdfFile file;
        QCOMPARE(file.open(path), PdfFile::OpenResult::Opened);
        QCOMPARE(file.title(), QFileInfo(path).completeBaseName());
    }

    void rendersWhitePaper()
    {
        PdfFile file;
        file.open(QStringLiteral(FIXTURES "/lease.pdf"));
        const QImage page = file.render(0, 612);
        QCOMPARE(page.width(), 612);
        QCOMPARE(page.height(), 792);
        QCOMPARE(page.pixelColor(5, 5), QColor(Qt::white));
        QVERIFY(file.render(7, 600).isNull());
    }

    void searchesEveryPage()
    {
        PdfFile file;
        file.open(QStringLiteral(FIXTURES "/lease.pdf"));
        const auto hits = file.search(QStringLiteral("DEPOSIT"));
        // The heading and two lines on page 2, one line on page 3.
        QCOMPARE(hits.size(), 4);
        QCOMPARE(hits[0].page, 1);
        QCOMPARE(hits[2].page, 1);
        QCOMPARE(hits[3].page, 2);
        for (const auto &hit : hits) {
            QCOMPARE(hit.rects.size(), 1);
            QVERIFY(QRectF(0, 0, 1, 1).contains(hit.rects.first()));
        }
        QVERIFY(file.search(QStringLiteral("  ")).isEmpty());
    }

    void selectsAWordAndARun()
    {
        PdfFile file;
        file.open(QStringLiteral(FIXTURES "/lease.pdf"));
        const auto hit = file.search(QStringLiteral("Rent is")).first();
        // Search finds the phrase; tap its first letters to find the word.
        const QRectF first = hit.rects.first();
        const QPointF onRent(first.left() + first.width() * 0.15, first.center().y());
        const TextRun word = file.wordAt(hit.page, onRent);
        QCOMPARE(word.text, QStringLiteral("Rent"));

        // Drag from "Rent" back to the line's start; ends swap into order.
        const QPointF start = file.search(QStringLiteral("3.2")).first().rects.first().center();
        const TextRun run = file.textBetween(hit.page, onRent, start);
        QCOMPARE(run.text, QStringLiteral("3.2 Rent"));
        QCOMPARE(run.rects.size(), 1);

        const QPointF lineTwoEnd = file.search(QStringLiteral("month.")).first().rects.first().center();
        const TextRun twoLines = file.textBetween(hit.page, start, lineTwoEnd);
        QVERIFY(twoLines.text.startsWith(QStringLiteral("3.2 Rent")));
        QVERIFY(twoLines.text.endsWith(QStringLiteral("month.")));
    }

    void walksAForm()
    {
        PdfFile file;
        file.open(QStringLiteral(FIXTURES "/form.pdf"));
        FormFields form(file);
        QCOMPARE(form.count(), 4);
        QCOMPARE(form.at(0).label, QStringLiteral("Full name"));
        QCOMPARE(form.at(1).label, QStringLiteral("New address"));
        QCOMPARE(form.at(3).kind, FormFields::Kind::Check);
        QCOMPARE(form.stillNeeded(), 3);
        QCOMPARE(form.firstMissing(), 0);

        QVERIFY(form.setText(0, QStringLiteral("Alex\nMorgan")));
        QCOMPARE(form.at(0).text, QStringLiteral("Alex Morgan"));
        QCOMPARE(form.stillNeeded(), 2);
        QCOMPARE(form.firstMissing(), 1);
        QCOMPARE(form.next(0), 1);
        QCOMPARE(form.next(3), 0);
        QCOMPARE(form.previous(0), 3);
        QVERIFY(form.setChecked(3, true));
        QVERIFY(form.at(3).checked);
        QVERIFY(!form.setChecked(0, true));
        QVERIFY(form.changed());
    }

    void savesFormAnswersAndMarks()
    {
        const QString path = copyOf(QStringLiteral("form.pdf"));
        PdfFile file;
        QCOMPARE(file.open(path), PdfFile::OpenResult::Opened);
        {
            FormFields form(file);
            form.setText(0, QStringLiteral("Alex Morgan"));
            form.setChecked(3, true);
        }
        Marks marks(file);
        const QString name = marks.addHighlight(0, {QRectF(0.1, 0.1, 0.3, 0.02)}, Marks::highlightColours().first());
        QVERIFY(name.startsWith(Marks::namePrefix()));
        QCOMPARE(Saver::save(file), Saver::Result::Saved);

        PdfFile again;
        QCOMPARE(again.open(path), PdfFile::OpenResult::Opened);
        FormFields form(again);
        QCOMPARE(form.at(0).text, QStringLiteral("Alex Morgan"));
        QVERIFY(form.at(3).checked);
        const auto highlights = Marks(again).highlights(0);
        QCOMPARE(highlights.size(), 1);
        QCOMPARE(highlights.first().name, name);
        QCOMPARE(highlights.first().color, Marks::highlightColours().first());
        QVERIFY(qAbs(highlights.first().rects.first().width() - 0.3) < 0.01);
        QCOMPARE(QFile::permissions(path) & kAnyone, QFile::ReadOwner | QFile::WriteOwner);
    }

    void removesAMark()
    {
        const QString path = copyOf(QStringLiteral("lease.pdf"));
        PdfFile file;
        file.open(path);
        Marks marks(file);
        const QString name = marks.addHighlight(1, file.search(QStringLiteral("deposit")).first().rects,
                                                Marks::highlightColours().at(2));
        QCOMPARE(marks.highlights(1).size(), 1);
        // Poppler draws the mark into the page picture straight away.
        const QRectF where = marks.highlights(1).first().rects.first();
        const QImage page = file.render(1, 612);
        const QColor under = page.pixelColor(int(where.left() * 612) + 1, int(where.top() * 792) + 1);
        QVERIFY2(under != QColor(Qt::white) && under.red() < 200, qPrintable(under.name()));
        QVERIFY(marks.remove(1, name));
        QVERIFY(marks.highlights(1).isEmpty());
        QVERIFY(!marks.remove(1, name));
    }

    void saveCopyLeavesTheOriginal()
    {
        const QString path = copyOf(QStringLiteral("form.pdf"));
        const QByteArray before = [&] { QFile f(path); f.open(QIODevice::ReadOnly); return f.readAll(); }();
        PdfFile file;
        file.open(path);
        FormFields(file).setText(1, QStringLiteral("18 Harbor St"));
        const QString copy = m_dir.filePath(QStringLiteral("copy.pdf"));
        QCOMPARE(Saver::saveCopy(file, copy), Saver::Result::Saved);
        QCOMPARE(Saver::saveCopy(file, path), Saver::Result::CannotWrite);
        const QByteArray after = [&] { QFile f(path); f.open(QIODevice::ReadOnly); return f.readAll(); }();
        QCOMPARE(after, before);
        PdfFile saved;
        QCOMPARE(saved.open(copy), PdfFile::OpenResult::Opened);
        QCOMPARE(FormFields(saved).at(1).text, QStringLiteral("18 Harbor St"));
    }

    void saveRefusesAFolderItCannotWrite()
    {
        QTemporaryDir locked;
        const QString path = locked.filePath(QStringLiteral("lease.pdf"));
        QFile::copy(QStringLiteral(FIXTURES "/lease.pdf"), path);
        PdfFile file;
        file.open(path);
        QFile::setPermissions(locked.path(), QFile::ReadOwner | QFile::ExeOwner);
        if (QFileInfo(locked.path()).isWritable())
            QSKIP("Running as a user who can write anywhere");
        QCOMPARE(Saver::save(file), Saver::Result::CannotWrite);
        QFile::setPermissions(locked.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }

    void keepsASignaturePrivately()
    {
        QTemporaryDir home;
        SignatureStore store(home.path());
        QVERIFY(!store.has());
        SignatureInk ink;
        ink.aspect = 2.5;
        ink.strokes = {{QPointF(0.1, 0.5), QPointF(0.1001, 0.5), QPointF(0.5, 0.2), QPointF(1.4, 0.8)},
                       {QPointF(0.3, 0.3)}};
        QVERIFY(store.save(ink));
        QCOMPARE(QFile::permissions(store.filePath()) & kAnyone, QFile::ReadOwner | QFile::WriteOwner);
        const SignatureInk back = store.load();
        QCOMPARE(back.aspect, 2.5);
        QCOMPARE(back.strokes.size(), 1);          // the one-point stroke is not ink
        QCOMPARE(back.strokes.first().size(), 3);  // the near-duplicate point went
        QCOMPARE(back.strokes.first().last(), QPointF(1.0, 0.8)); // kept on the pad
        QVERIFY(store.forget());
        QVERIFY(!store.has());
        QVERIFY(!store.save(SignatureInk{}));
    }

    void placesASignatureWithoutStretching()
    {
        const QString path = copyOf(QStringLiteral("lease.pdf"));
        PdfFile file;
        file.open(path);
        SignatureInk ink;
        ink.aspect = 2.0;
        ink.strokes = {{QPointF(0, 0), QPointF(1, 1)}};
        Marks marks(file);
        const QString name = marks.placeSignature(2, QRectF(0.1, 0.5, 0.5, 0.05), ink);
        QVERIFY(!name.isEmpty());
        QCOMPARE(Saver::save(file), Saver::Result::Saved);

        PdfFile again;
        again.open(path);
        const auto page = again.page(2);
        bool found = false;
        for (const auto &a : page->annotations({Poppler::Annotation::AInk})) {
            if (a->uniqueName() != name)
                continue;
            found = true;
            const QRectF r = a->boundary();
            const double w = r.width() * 612, h = r.height() * 792;
            QVERIFY(qAbs(w / h - 2.0) < 0.15);
            QVERIFY(qAbs(r.center().x() - 0.35) < 0.02);
        }
        QVERIFY(found);
    }

    void remembersThePage()
    {
        QTemporaryDir home;
        ReadingPlace places(home.filePath(QStringLiteral("places.ini")));
        QCOMPARE(places.page(QStringLiteral("/tmp/a.pdf")), 0);
        places.remember(QStringLiteral("/tmp/a.pdf"), 4);
        QCOMPARE(places.page(QStringLiteral("/tmp/a.pdf")), 4);
        QFile ini(home.filePath(QStringLiteral("places.ini")));
        ini.open(QIODevice::ReadOnly);
        QVERIFY(!ini.readAll().contains("a.pdf"));
        for (int i = 0; i < ReadingPlace::kept + 5; ++i)
            places.remember(QStringLiteral("/tmp/%1.pdf").arg(i), 1);
        QSettings check(home.filePath(QStringLiteral("places.ini")), QSettings::IniFormat);
        check.beginGroup(QStringLiteral("places"));
        QCOMPARE(check.childGroups().size(), ReadingPlace::kept);
    }
};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
