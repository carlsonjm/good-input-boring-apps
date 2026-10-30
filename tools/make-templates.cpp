// Not a test: builds the ready-made sheets Sheets offers to start from, using
// the engine itself, so they are real spreadsheets of the engine's own making.
//   office-templates <blank.fods> <out folder>
#include "Engine.h"
#include "OfficeDocument.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTest>
#include <cstdio>

namespace {
// com.sun.star.awt.Key.RETURN
constexpr int Return = 1280;

// Fills cells to the right of start, one at a time, leaving empty ones alone.
void row(OfficeDocument &doc, const QString &start, const QStringList &cells)
{
    const QChar column = start.at(0);
    const QString number = start.mid(1);
    for (int i = 0; i < cells.size(); ++i) {
        if (cells[i].isEmpty()) {
            continue;
        }
        doc.command(".uno:GoToCell", {{"ToPoint", QString(QChar(column.unicode() + i)) + number}});
        doc.typeText(cells[i]);
        doc.pressKey(Return);
        doc.settle(30);
    }
}

void on(OfficeDocument &doc, const QString &range, const QString &command, const QVariantMap &args = {})
{
    doc.command(".uno:GoToCell", {{"ToPoint", range}});
    doc.command(command, args);
    doc.settle(50);
}

void header(OfficeDocument &doc, const QString &range, int colour)
{
    on(doc, range, ".uno:Bold");
    on(doc, range, ".uno:BackgroundColor", {{"BackgroundColor", colour}});
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString blank = QString::fromLocal8Bit(argv[1]);
    const QDir out(QString::fromLocal8Bit(argv[2]));
    auto make = [&](const QString &name, const QString &sheet, const std::function<void(OfficeDocument &)> &fill) {
        const QString work = out.filePath(QStringLiteral("work.fods"));
        QFile::remove(work);
        QFile::copy(blank, work);
        QFile::setPermissions(work, QFile::ReadOwner | QFile::WriteOwner);
        OfficeDocument doc;
        doc.setAutosave(false);
        doc.setSource(QUrl::fromLocalFile(work));
        QTest::qWaitFor([&] { return doc.status() == OfficeDocument::Ready; }, 60000);
        doc.command(".uno:RenameTable", {{"Name", sheet}});
        fill(doc);
        doc.command(".uno:GoToCell", {{"ToPoint", "A2"}});
        doc.settle(200);
        const QString target = out.filePath(name + QStringLiteral(".fods"));
        QFile::remove(target);
        std::printf("%s: %s\n", qPrintable(name), doc.saveCopy(QUrl::fromLocalFile(target), "fods") ? "ok" : "FAILED");
        doc.setSource(QUrl());
        QFile::remove(work);
    };

    make("budget", "Budget", [](OfficeDocument &doc) {
        row(doc, "A1", {"Item", "Category", "Spent", "Budget", "Left"});
        for (int r = 2; r <= 9; ++r) {
            row(doc, QStringLiteral("E%1").arg(r), {QStringLiteral("=D%1-C%1").arg(r)});
        }
        row(doc, "A10", {"Total", "", "=SUM(C2:C9)", "=SUM(D2:D9)", "=SUM(E2:E9)"});
        header(doc, "A1:E1", 0x9DD6F2);
        on(doc, "A10:E10", ".uno:Bold");
        on(doc, "C2:E10", ".uno:NumberFormatCurrency");
        on(doc, "A1:A10", ".uno:ColumnWidth", {{"ColumnWidth", 4500}});
        on(doc, "B1:E10", ".uno:ColumnWidth", {{"ColumnWidth", 3000}});
    });
    make("tracker", "Tracker", [](OfficeDocument &doc) {
        row(doc, "A1", {"Date", "Task", "Done", "Notes"});
        header(doc, "A1:D1", 0xA8DDB5);
        on(doc, "A2:A60", ".uno:NumberFormatDate");
        on(doc, "A1:A60", ".uno:ColumnWidth", {{"ColumnWidth", 3000}});
        on(doc, "B1:B60", ".uno:ColumnWidth", {{"ColumnWidth", 6000}});
        on(doc, "C1:C60", ".uno:ColumnWidth", {{"ColumnWidth", 2000}});
        on(doc, "D1:D60", ".uno:ColumnWidth", {{"ColumnWidth", 7000}});
    });
    make("shopping", "Shopping", [](OfficeDocument &doc) {
        row(doc, "A1", {"Item", "Shop", "How many", "Price", "Cost"});
        for (int r = 2; r <= 15; ++r) {
            row(doc, QStringLiteral("E%1").arg(r), {QStringLiteral("=IF(D%1=\"\";\"\";C%1*D%1)").arg(r)});
        }
        row(doc, "D16", {"Total", "=SUM(E2:E15)"});
        header(doc, "A1:E1", 0xF7D774);
        on(doc, "D16:E16", ".uno:Bold");
        on(doc, "D2:E16", ".uno:NumberFormatCurrency");
        on(doc, "A1:A16", ".uno:ColumnWidth", {{"ColumnWidth", 5000}});
        on(doc, "B1:B16", ".uno:ColumnWidth", {{"ColumnWidth", 3500}});
    });
    make("schedule", "Week", [](OfficeDocument &doc) {
        row(doc, "A1", {"", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"});
        for (int h = 7; h <= 21; ++h) {
            row(doc, QStringLiteral("A%1").arg(h - 5), {QStringLiteral("%1:00").arg(h)});
        }
        header(doc, "A1:H1", 0xD2B4F5);
        on(doc, "A2:A16", ".uno:Bold");
        on(doc, "B1:H16", ".uno:ColumnWidth", {{"ColumnWidth", 3200}});
    });

    Engine::instance()->shutdown();
    std::fflush(nullptr);
    std::_Exit(0);
}
