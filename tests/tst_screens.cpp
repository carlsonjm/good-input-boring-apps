// Checks the screens as drawn, the way a person would see them: the window is
// laid out and painted without a screen, captured as a picture, and the
// picture is measured. Run at the tablet's scaling as well as at 1x (see
// CMakeLists.txt); with OFFICE_SHOTS set, every capture is also kept there
// for looking at.
#include "Engine.h"
#include "OfficeDocument.h"
#include "Recent.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

Q_IMPORT_QML_PLUGIN(co_goodinput_officePlugin)

class ScreensTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QUrl copyOf(const QString &path, const QString &as)
    {
        const QString target = m_dir.filePath(as);
        QFile::remove(target);
        if (!QFile::copy(path, target)) {
            qFatal("could not copy %s", qPrintable(path));
        }
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
        return QUrl::fromLocalFile(target);
    }

    // One window, as the program opens it.
    struct Screen
    {
        std::unique_ptr<QQmlApplicationEngine> engine;
        QQuickWindow *window = nullptr;
        OfficeDocument *doc = nullptr;

        QQuickItem *item(const char *name) const { return window->findChild<QQuickItem *>(QString::fromLatin1(name)); }
        qreal dpr() const { return window->effectiveDevicePixelRatio(); }
        // An item's place in the window, in the picture's pixels.
        QRectF inPicture(QQuickItem *item) const
        {
            const QRectF r = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
            return QRectF(r.x() * dpr(), r.y() * dpr(), r.width() * dpr(), r.height() * dpr());
        }
    };

    Screen open(const QString &kind, const QUrl &file)
    {
        Screen s;
        s.engine = std::make_unique<QQmlApplicationEngine>();
        s.engine->setInitialProperties({{QStringLiteral("kind"), kind}, {QStringLiteral("startFile"), file}});
        s.engine->load(QUrl(QStringLiteral("qrc:/co/goodinput/office/DocsWindow.qml")));
        s.window = qobject_cast<QQuickWindow *>(s.engine->rootObjects().value(0));
        if (!s.window) {
            return s;
        }
        s.window->resize(1280, 800);
        s.window->show();
        s.doc = s.window->findChild<OfficeDocument *>();
        if (s.doc && !file.isEmpty()) {
            if (!QTest::qWaitFor([&] { return s.doc->status() == OfficeDocument::Ready; }, 30000)) {
                s.window = nullptr;
            }
        }
        return s;
    }

    // Waits for the tiles to come in and the window to settle, then keeps
    // a picture of it.
    QImage capture(Screen &s, const QString &name)
    {
        if (s.doc) {
            s.doc->settle(300);
        }
        QTest::qWait(1200);
        QImage picture = s.window->grabWindow();
        const QString shots = qEnvironmentVariable("OFFICE_SHOTS");
        if (!shots.isEmpty()) {
            QDir().mkpath(shots);
            picture.save(QStringLiteral("%1/%2@%3.png").arg(shots, name).arg(s.dpr()));
        }
        return picture;
    }

    // The engine's grid lines: grey, darker than the white of the cells and
    // lighter than text. Walks one line of pixels and returns where they are.
    static bool gridGrey(QRgb c)
    {
        const int r = qRed(c), g = qGreen(c), b = qBlue(c);
        return qAbs(r - g) < 12 && qAbs(g - b) < 12 && r > 140 && r < 236;
    }
    static QList<int> linesDown(const QImage &picture, int x, int from, int to)
    {
        QList<int> found;
        for (int y = qMax(0, from); y < qMin(picture.height(), to); ++y) {
            if (gridGrey(picture.pixel(x, y)) && (found.isEmpty() || y - found.last() > 2)) {
                found << y;
            }
        }
        return found;
    }
    static QList<int> linesAcross(const QImage &picture, int y, int from, int to)
    {
        QList<int> found;
        for (int x = qMax(0, from); x < qMin(picture.width(), to); ++x) {
            if (gridGrey(picture.pixel(x, y)) && (found.isEmpty() || x - found.last() > 2)) {
                found << x;
            }
        }
        return found;
    }
    static int nearest(const QList<int> &lines, qreal at)
    {
        int best = -100000;
        for (int line : lines) {
            if (qAbs(line - at) < qAbs(best - at)) {
                best = line;
            }
        }
        return best;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }


    // The ring round the chosen cell sits on the grid lines the engine drew,
    // near the top of the sheet and far down it.
    void sheetRingSitsOnTheGrid_data()
    {
        QTest::addColumn<QString>("cell");
        QTest::newRow("near the top") << QStringLiteral("B3");
        QTest::newRow("far down") << QStringLiteral("F22");
        QTest::newRow("wide column") << QStringLiteral("A18");
    }
    void sheetRingSitsOnTheGrid()
    {
        QFETCH(QString, cell);
        Screen s = open(QStringLiteral("spreadsheet"), copyOf(QStringLiteral(":/templates/shopping.fods"), QStringLiteral("ring.fods")));
        QVERIFY(s.window);
        s.doc->command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), cell}});
        QTRY_COMPARE(s.doc->cellAddress(), cell);
        const QImage picture = capture(s, QStringLiteral("ring-") + cell);
        QQuickItem *ring = s.item("cellRing");
        QVERIFY(ring && ring->isVisible());
        // The ring is drawn one point outside the cell, centred on its edges.
        const QRectF r = s.inPicture(ring).adjusted(s.dpr(), s.dpr(), -s.dpr(), -s.dpr());
        const int left = int(r.left() - 6 * s.dpr());
        const QList<int> rows = linesDown(picture, left, int(r.top() - 80 * s.dpr()), int(r.bottom() + 80 * s.dpr()));
        const int above = int(r.top() - 6 * s.dpr());
        const QList<int> columns = linesAcross(picture, above, int(r.left() - 300 * s.dpr()), int(r.right() + 300 * s.dpr()));
        const qreal slack = 1.6 * s.dpr();
        QVERIFY2(qAbs(nearest(rows, r.top()) - r.top()) <= slack,
                 qPrintable(QStringLiteral("ring top %1, grid lines %2").arg(r.top()).arg(QString::number(nearest(rows, r.top())))));
        QVERIFY2(qAbs(nearest(rows, r.bottom()) - r.bottom()) <= slack,
                 qPrintable(QStringLiteral("ring bottom %1, grid line %2").arg(r.bottom()).arg(nearest(rows, r.bottom()))));
        QVERIFY2(qAbs(nearest(columns, r.left()) - r.left()) <= slack,
                 qPrintable(QStringLiteral("ring left %1, grid line %2").arg(r.left()).arg(nearest(columns, r.left()))));
        QVERIFY2(qAbs(nearest(columns, r.right()) - r.right()) <= slack,
                 qPrintable(QStringLiteral("ring right %1, grid line %2").arg(r.right()).arg(nearest(columns, r.right()))));
    }

    // Each row number and column letter spans the same cells as the grid.
    void headersMatchTheGrid()
    {
        Screen s = open(QStringLiteral("spreadsheet"), copyOf(QStringLiteral(":/templates/shopping.fods"), QStringLiteral("headers.fods")));
        QVERIFY(s.window);
        // The ring, out of the way, so it hides no grid line.
        s.doc->command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), QStringLiteral("Z200")}});
        QTRY_COMPARE(s.doc->cellAddress(), QStringLiteral("Z200"));
        const QImage picture = capture(s, QStringLiteral("headers"));
        QQuickItem *headers = s.item("sheetHeaders");
        QVERIFY(headers);
        const QRectF h = s.inPicture(headers);
        const qreal band = headers->property("band").toReal() * s.dpr();
        const qreal gutter = headers->property("gutter").toReal() * s.dpr();
        const QList<int> rows = linesDown(picture, int(h.left() + gutter + 6 * s.dpr()), int(h.top() + band + 2), int(h.bottom() - 2));
        // Across a few rows down, below the template's coloured first row.
        const QList<int> columns = linesAcross(picture, int(h.top() + band + 100 * s.dpr()), int(h.left() + gutter + 2), int(h.right() - 2));
        QVERIFY(rows.size() > 10);
        QVERIFY(columns.size() > 3);
        int checked = 0;
        for (const QVariant &v : headers->property("rows").toList()) {
            const QVariantMap row = v.toMap();
            const qreal end = h.top() + band + (row.value(QStringLiteral("from")).toReal() + row.value(QStringLiteral("size")).toReal()) * s.dpr();
            // The template's first row is coloured, with no grey line under it.
            if (row.value(QStringLiteral("text")).toInt() < 2 || end > h.bottom() - 4) {
                continue;
            }
            QVERIFY2(qAbs(nearest(rows, end) - end) <= 1.6 * s.dpr(),
                     qPrintable(QStringLiteral("row %1 ends at %2, grid line %3").arg(row.value(QStringLiteral("text")).toString()).arg(end).arg(nearest(rows, end))));
            ++checked;
        }
        for (const QVariant &v : headers->property("columns").toList()) {
            const QVariantMap column = v.toMap();
            const qreal end = h.left() + gutter + (column.value(QStringLiteral("from")).toReal() + column.value(QStringLiteral("size")).toReal()) * s.dpr();
            if (end < h.left() + gutter + 4 || end > h.right() - 4) {
                continue;
            }
            QVERIFY2(qAbs(nearest(columns, end) - end) <= 1.6 * s.dpr(),
                     qPrintable(QStringLiteral("column %1 ends at %2, grid line %3").arg(column.value(QStringLiteral("text")).toString()).arg(end).arg(nearest(columns, end))));
            ++checked;
        }
        QVERIFY(checked > 12);
    }

    // A tap in the middle of a cell, as drawn, chooses that cell.
    void tapChoosesTheCellSeen()
    {
        Screen s = open(QStringLiteral("spreadsheet"), copyOf(QStringLiteral(":/templates/shopping.fods"), QStringLiteral("tap.fods")));
        QVERIFY(s.window);
        capture(s, QStringLiteral("tap"));
        QQuickItem *headers = s.item("sheetHeaders");
        const QRectF h = s.inPicture(headers);
        const qreal band = headers->property("band").toReal() * s.dpr();
        const qreal gutter = headers->property("gutter").toReal() * s.dpr();
        // The middle of C21 as the row numbers and column letters place it,
        // which headersMatchTheGrid holds to the grid lines drawn.
        QPointF middle;
        for (const QVariant &v : headers->property("rows").toList()) {
            const QVariantMap row = v.toMap();
            if (row.value(QStringLiteral("text")).toString() == QLatin1String("21")) {
                middle.setY(h.top() / s.dpr() + band / s.dpr() + row.value(QStringLiteral("from")).toReal() + row.value(QStringLiteral("size")).toReal() / 2);
            }
        }
        for (const QVariant &v : headers->property("columns").toList()) {
            const QVariantMap column = v.toMap();
            if (column.value(QStringLiteral("text")).toString() == QLatin1String("C")) {
                middle.setX(h.left() / s.dpr() + gutter / s.dpr() + column.value(QStringLiteral("from")).toReal() + column.value(QStringLiteral("size")).toReal() / 2);
            }
        }
        QVERIFY(middle.x() > 0 && middle.y() > 0);
        QTest::mouseClick(s.window, Qt::LeftButton, Qt::NoModifier, middle.toPoint());
        QTRY_COMPARE_WITH_TIMEOUT(s.doc->cellAddress(), QStringLiteral("C21"), 5000);
    }

    // With Aa open the sheet steps aside: nothing of it shows past the note.
    void sheetStepsAsideForAa()
    {
        Screen s = open(QStringLiteral("spreadsheet"), copyOf(QStringLiteral(":/templates/budget.fods"), QStringLiteral("aa.fods")));
        QVERIFY(s.window);
        s.window->setProperty("styleOpen", true);
        capture(s, QStringLiteral("sheets-aa"));
        QQuickItem *headers = s.item("sheetHeaders");
        QQuickItem *note = s.item("cellSheet");
        QVERIFY(headers && note && note->isVisible());
        QVERIFY2(s.inPicture(headers).right() <= s.inPicture(note).left(),
                 qPrintable(QStringLiteral("sheet ends at %1, Aa starts at %2").arg(s.inPicture(headers).right()).arg(s.inPicture(note).left())));
    }

    // Share of a picture's points, in from an item's edges, that are a
    // colour rather than white, grey or black.
    static qreal colouredShare(const Screen &s, const QImage &picture, QQuickItem *item)
    {
        const QRect area = s.inPicture(item).toRect().adjusted(40, 40, -40, -40) & picture.rect();
        int coloured = 0, all = 0;
        for (int y = area.top(); y < area.bottom(); y += 3) {
            for (int x = area.left(); x < area.right(); x += 3) {
                const QColor c = QColor::fromRgb(picture.pixel(x, y));
                ++all;
                if (c.hsvSaturation() > 60 && c.value() > 40) {
                    ++coloured;
                }
            }
        }
        return all ? qreal(coloured) / all : 1;
    }

    // Slides are white with their titles as plain words: no filled boxes
    // behind a title or its text, on a new deck, on a slide added to it, or
    // on a deck that names no fill of its own.
    void slidesHaveNoFilledBoxes_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<bool>("addSlide");
        QTest::newRow("new deck") << QString() << false;
        QTest::newRow("new deck, new slide") << QString() << true;
        QTest::newRow("deck.fodp") << QStringLiteral(FIXTURES "/deck.fodp") << false;
        QTest::newRow("deck.pptx") << QStringLiteral(FIXTURES "/deck.pptx") << false;
    }
    void slidesHaveNoFilledBoxes()
    {
        QFETCH(QString, file);
        QFETCH(bool, addSlide);
        QString source = file;
        if (source.isEmpty()) {
            Recent recent;
            recent.setKind(QStringLiteral("presentation"));
            source = recent.blankSource().toLocalFile();
        }
        Screen s = open(QStringLiteral("presentation"), copyOf(source, QStringLiteral("filled.") + QFileInfo(source).suffix()));
        QVERIFY(s.window);
        if (addSlide) {
            const int before = s.doc->parts();
            s.doc->command(QStringLiteral(".uno:InsertPage"));
            QVERIFY(QTest::qWaitFor([&] { return s.doc->parts() == before + 1; }, 10000));
            s.doc->setPart(before);
            QVERIFY(QTest::qWaitFor([&] { return s.doc->currentPart() == before; }, 10000));
        }
        const QImage picture = capture(s, QStringLiteral("slides-") + QString::fromLatin1(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('-')).remove(QLatin1Char(',')));
        QQuickItem *paper = s.item("paper");
        QVERIFY(paper);
        const qreal share = colouredShare(s, picture, paper);
        QVERIFY2(share < 0.005, qPrintable(QStringLiteral("%1% of the slide coloured").arg(share * 100, 0, 'f', 1)));
    }

    // Round a slide there is only desk: no line or sliver of the engine's
    // own drawing shows past the rounded edge.
    void slideEdgesAreClean()
    {
        Screen s = open(QStringLiteral("presentation"), copyOf(QStringLiteral(FIXTURES "/deck.pptx"), QStringLiteral("edges.pptx")));
        QVERIFY(s.window);
        const QImage picture = capture(s, QStringLiteral("slides-edges"));
        QQuickItem *paper = s.item("paper");
        const QColor desk = QColor::fromRgb(picture.pixel(int(s.inPicture(paper).left() + 4), int(s.inPicture(paper).top() + 4)));
        // The slide's own rectangle, found as the white within the paper.
        const QRect within = s.inPicture(paper).toRect() & picture.rect();
        QRect slide;
        for (int y = within.top(); y < within.bottom(); ++y) {
            for (int x = within.left(); x < within.right(); x += 2) {
                if (picture.pixel(x, y) == qRgb(255, 255, 255)) {
                    slide |= QRect(x, y, 1, 1);
                }
            }
        }
        QVERIFY(slide.width() > 200);
        // A band a few pixels wide just outside the slide: desk, or the
        // white of the slide blended into it at the curve, and nothing darker.
        int stray = 0;
        const QRect outside = slide.adjusted(-6, -6, 6, 6) & picture.rect();
        for (int y = outside.top(); y <= outside.bottom(); ++y) {
            for (int x = outside.left(); x <= outside.right(); ++x) {
                if (slide.adjusted(4, 4, -4, -4).contains(x, y)) {
                    continue;
                }
                const QColor c = QColor::fromRgb(picture.pixel(x, y));
                if (c.lightness() < desk.lightness() - 6) {
                    ++stray;
                }
            }
        }
        QVERIFY2(stray == 0, qPrintable(QStringLiteral("%1 stray pixels round the slide").arg(stray)));
    }

    // With a dark colour scheme the desk is dark.
    void followsADarkScheme()
    {
        const QPalette before = QGuiApplication::palette();
        QPalette dark = before;
        dark.setColor(QPalette::Window, QColor(0x20, 0x20, 0x20));
        dark.setColor(QPalette::WindowText, Qt::white);
        QGuiApplication::setPalette(dark);
        Screen s = open(QStringLiteral("text"), QUrl());
        QVERIFY(s.window);
        const QImage picture = capture(s, QStringLiteral("home-dark"));
        QGuiApplication::setPalette(before);
        const QColor corner = QColor::fromRgb(picture.pixel(4, picture.height() / 2));
        QVERIFY2(corner.value() < 60, qPrintable(corner.name()));
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    ScreensTest test;
    const int code = QTest::qExec(&test, argc, argv);
    Engine::instance()->shutdown();
    std::fflush(nullptr);
    std::_Exit(code);
}

#include "tst_screens.moc"
