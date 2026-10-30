// Opens a window as the program does, plays a few steps on its document and
// saves a picture of it, so the screens can be looked at without a screen.
//   office-snap <docs|sheets|slides> <file> <step> <picture>
// Steps: open, select (double tap the first word), style (open Aa), home,
// gap (where the first page meets the second), cell (B4 chosen), page (Aa's
// Page side), find (looking for \"the\"), more (the More menu).
#include "Engine.h"
#include "OfficeDocument.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>
#include <cstdlib>

Q_IMPORT_QML_PLUGIN(co_goodinput_officePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    if (argc < 5) {
        std::fprintf(stderr, "office-snap <docs|sheets|slides> <file> <step> <picture>\n");
        return 2;
    }
    const QString app_ = QString::fromLocal8Bit(argv[1]);
    const QString kind = app_ == QLatin1String("sheets") ? QStringLiteral("spreadsheet")
        : app_ == QLatin1String("slides") ? QStringLiteral("presentation") : QStringLiteral("text");
    const QString step = QString::fromLocal8Bit(argv[3]);
    const QUrl file = step == QLatin1String("home") ? QUrl() : QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2]));
    int code = 1;
    {
        QQmlApplicationEngine engine;
        engine.setInitialProperties({{QStringLiteral("kind"), kind}, {QStringLiteral("startFile"), file}});
        engine.load(QUrl(QStringLiteral("qrc:/co/goodinput/office/DocsWindow.qml")));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
        if (window) {
            auto *doc = window->findChild<OfficeDocument *>();
            if (!file.isEmpty()) {
                QTest::qWaitFor([&] { return doc->status() == OfficeDocument::Ready; }, 30000);
            }
            QTest::qWait(800);
            if (step == QLatin1String("select")) {
                doc->command(QStringLiteral(".uno:GoToStartOfDoc"));
                doc->settle(300);
                QTest::qWait(200);
                const QRectF at = doc->cursorRect();
                doc->tap(at.x() + 300, at.center().y(), 2);
                doc->settle(300);
            } else if (step == QLatin1String("style")) {
                window->setProperty("styleOpen", true);
            } else if (step == QLatin1String("gap")) {
                // Scrolls to where the first page meets the second.
                for (QObject *o : window->findChildren<QObject *>()) {
                    if (QByteArray(o->metaObject()->className()).contains("Flickable"))
                        o->setProperty("contentY", 900);
                }
            } else if (step == QLatin1String("page")) {
                window->setProperty("styleOpen", true);
                QTest::qWait(300);
                for (QObject *o : window->findChildren<QObject *>()) {
                    if (o->property("side").isValid())
                        o->setProperty("side", QStringLiteral("page"));
                }
            } else if (step == QLatin1String("find")) {
                QMetaObject::invokeMethod(window, "startFinding");
                QObject *bar = window->findChild<QObject *>(QStringLiteral("findBar"));
                bar->setProperty("needle", QStringLiteral("the"));
                QMetaObject::invokeMethod(bar, "search");
                QMetaObject::invokeMethod(bar, "step", Q_ARG(QVariant, false));
            } else if (step == QLatin1String("more")) {
                QMetaObject::invokeMethod(window->findChild<QObject *>(QStringLiteral("moreMenu")), "open");
            } else if (step == QLatin1String("cell")) {
                doc->command(QStringLiteral(".uno:GoToCell"), {{QStringLiteral("ToPoint"), QStringLiteral("B4")}});
                doc->settle(300);
            }
            QTest::qWait(1500);
            code = window->grabWindow().save(QString::fromLocal8Bit(argv[4])) ? 0 : 1;
        }
    }
    Engine::instance()->shutdown();
    std::fflush(nullptr);
    std::_Exit(code);
}
