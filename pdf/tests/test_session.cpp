#include "PageImages.h"
#include "Session.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SessionTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QUrl copyOf(const QString &name)
    {
        const QString target = m_dir.filePath(QStringLiteral("%1-%2").arg(++m_copies).arg(name));
        QFile::copy(QStringLiteral(FIXTURES "/") + name, target);
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
        return QUrl::fromLocalFile(target);
    }
    int m_copies = 0;

    std::unique_ptr<Session> session()
    {
        return std::make_unique<Session>(m_dir.filePath(QStringLiteral("data")),
                                         m_dir.filePath(QStringLiteral("places.ini")));
    }

private Q_SLOTS:
    void opensAndSaysWhy()
    {
        auto s = session();
        QCOMPARE(s->status(), QStringLiteral("empty"));
        QVERIFY(!s->open(QUrl::fromLocalFile(QStringLiteral(FIXTURES "/not-a-pdf.pdf"))));
        QCOMPARE(s->status(), QStringLiteral("notPdf"));
        QSignalSpy opened(s.get(), &Session::opened);
        QVERIFY(s->open(QUrl::fromLocalFile(QStringLiteral(FIXTURES "/lease.pdf"))));
        QCOMPARE(opened.count(), 1);
        QCOMPARE(s->status(), QStringLiteral("open"));
        QCOMPARE(s->pageCount(), 3);
        QVERIFY(qAbs(s->pageAspect(0) - 792.0 / 612.0) < 1e-6);
    }

    void returnsToThePageLeftOn()
    {
        const QUrl url = copyOf(QStringLiteral("lease.pdf"));
        {
            auto s = session();
            s->open(url);
            s->setPage(2);
        }
        auto s = session();
        s->open(url);
        QCOMPARE(s->page(), 2);
        s->setPage(40);
        QCOMPARE(s->page(), 2);
    }

    void selectsCopiesAndHighlights()
    {
        auto s = session();
        s->open(copyOf(QStringLiteral("lease.pdf")));
        s->search(QStringLiteral("deposit"));
        QCOMPARE(s->searchHits().size(), 4);
        const QVariantList pages = s->pagesWithHits();
        QCOMPARE(pages.size(), 2);
        QCOMPARE(pages[0].toMap().value(QStringLiteral("count")).toInt(), 3);

        const QVariantMap hit = s->searchHits().at(1).toMap();
        const double x = hit[QStringLiteral("x")].toDouble() + hit[QStringLiteral("w")].toDouble() / 2;
        const double y = hit[QStringLiteral("y")].toDouble() + hit[QStringLiteral("h")].toDouble() / 2;
        const QVariantMap word = s->wordAt(1, x, y);
        QCOMPARE(word[QStringLiteral("text")].toString(), QStringLiteral("deposit"));
        s->copy(word[QStringLiteral("text")].toString());
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("deposit"));

        QSignalSpy redraw(s.get(), &Session::revisionChanged);
        const QString name = s->highlight(1, word[QStringLiteral("rects")].toList(), 1);
        QVERIFY(!name.isEmpty());
        QVERIFY(s->changed());
        QCOMPARE(redraw.count(), 1);
        QCOMPARE(s->highlights(1).size(), 1);
        QVERIFY(s->highlights(1).first().toMap()[QStringLiteral("ours")].toBool());
        QVERIFY(s->highlight(1, word[QStringLiteral("rects")].toList(), 9).isEmpty());
        QVERIFY(s->removeMark(1, name));
        QVERIFY(s->highlights(1).isEmpty());
    }

    void fillsAndSavesAForm()
    {
        const QUrl url = copyOf(QStringLiteral("form.pdf"));
        auto s = session();
        s->open(url);
        QCOMPARE(s->fieldCount(), 4);
        QCOMPARE(s->stillNeeded(), 3);
        QCOMPARE(s->firstEmptyField(), 0);
        QCOMPARE(s->fieldsOnPage(0).size(), 4);
        QCOMPARE(s->field(3)[QStringLiteral("kind")].toString(), QStringLiteral("check"));
        QSignalSpy form(s.get(), &Session::formChanged);
        QVERIFY(s->setFieldText(0, QStringLiteral("Alex Morgan")));
        QCOMPARE(form.count(), 1);
        QCOMPARE(s->stillNeeded(), 2);
        QCOMPARE(s->nextField(0), 1);
        QCOMPARE(s->save(), QStringLiteral("saved"));
        QVERIFY(!s->changed());
        QCOMPARE(s->field(0)[QStringLiteral("text")].toString(), QStringLiteral("Alex Morgan"));
        // Still editable after a save: the session follows the saved file.
        QVERIFY(s->setFieldText(1, QStringLiteral("18 Harbor St")));
        QCOMPARE(s->save(), QStringLiteral("saved"));
    }

    void signsWithAStoredSignature()
    {
        auto s = session();
        s->open(copyOf(QStringLiteral("lease.pdf")));
        QVERIFY(!s->hasSignature());
        QVERIFY(s->placeSignature(2, 0.1, 0.5, 0.4, 0.06).isEmpty());
        const QVariantList stroke{QVariantMap{{"x", 0.1}, {"y", 0.6}}, QVariantMap{{"x", 0.5}, {"y", 0.2}},
                                  QVariantMap{{"x", 0.9}, {"y", 0.7}}};
        QSignalSpy changed(s.get(), &Session::signatureChanged);
        QVERIFY(s->saveSignature({QVariant(stroke)}, 2.6));
        QCOMPARE(changed.count(), 1);
        QVERIFY(s->hasSignature());
        QCOMPARE(s->signatureStrokes().size(), 1);
        QCOMPARE(s->signatureAspect(), 2.6);
        QVERIFY(!s->placeSignature(2, 0.1, 0.5, 0.4, 0.06).isEmpty());
        QVERIFY(s->forgetSignature());
        QVERIFY(!s->hasSignature());
    }

    void drawsPagesForQml()
    {
        auto s = session();
        s->open(QUrl::fromLocalFile(QStringLiteral(FIXTURES "/lease.pdf")));
        PageImages images(*s);
        QSize size;
        const QImage page = images.requestImage(QStringLiteral("3/1"), &size, QSize(300, 0));
        QCOMPARE(size, QSize(300, 388));
        QCOMPARE(page.size(), size);
        QVERIFY(images.requestImage(QStringLiteral("nonsense"), &size, QSize(300, 0)).isNull());
        QCOMPARE(images.requestImage(QStringLiteral("1/0"), &size, QSize(99999, 0)).width(), PageImages::widest);
    }
};

QTEST_MAIN(SessionTest)
#include "test_session.moc"
