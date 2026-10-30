#include "Glyphs.h"
#include "Look.h"
#include "PageImages.h"
#include "Session.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("good-input-pdf"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("PDF"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("goodinput.co"));
    QGuiApplication::setDesktopFileName(QStringLiteral(GOODINPUT_PDF_APP_ID));
    QGuiApplication::setApplicationVersion(QStringLiteral(GOODINPUT_PDF_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("The PDF to open."));
    parser.process(app);

    Session session;
    Look look;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("page"), new PageImages(session));
    engine.addImageProvider(QStringLiteral("glyph"), new Glyphs);
    engine.rootContext()->setContextProperty(QStringLiteral("pdf"), &session);
    engine.rootContext()->setContextProperty(QStringLiteral("look"), &look);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/GoodInputPdf/qml/Main.qml")));

    if (!parser.positionalArguments().isEmpty())
        session.open(QUrl::fromUserInput(parser.positionalArguments().first(), QDir::currentPath(),
                                         QUrl::AssumeLocalFile));
    // A person who closes the window keeps what they wrote, as on paper.
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &session, [&session] {
        if (session.changed())
            session.save();
        session.close();
    });
    // For checks: GOODINPUT_PDF_SHOT=<png> draws the window once it has
    // settled, saves the picture and quits. GOODINPUT_PDF_SHOT_DO names a
    // QML function on the window's app item to call first, such as openSign.
    if (const QString shot = qEnvironmentVariable("GOODINPUT_PDF_SHOT"); !shot.isEmpty()) {
        QTimer::singleShot(800, &app, [&engine] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
            if (!window)
                return;
            const QString act = qEnvironmentVariable("GOODINPUT_PDF_SHOT_DO");
            if (!act.isEmpty()) {
                QObject *item = window->property("app").value<QObject *>();
                for (const QString &call : act.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                    const QStringList parts = call.split(QLatin1Char(':'));
                    QVariantList args;
                    for (int i = 1; i < parts.size(); ++i) {
                        bool number = false;
                        const double n = parts[i].toDouble(&number);
                        args.append(number ? QVariant(n) : QVariant(parts[i]));
                    }
                    QMetaObject::invokeMethod(item, "callFromCheck", Q_ARG(QVariant, parts[0]), Q_ARG(QVariant, args));
                }
            }
        });
        QTimer::singleShot(2200, &app, [&engine, shot] {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0)))
                window->grabWindow().save(shot);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
