#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>

#include "Engine.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

Q_IMPORT_QML_PLUGIN(co_goodinput_officePlugin)

// One program, three apps: --docs, --sheets or --slides picks which, and so
// which name and icon the desktop shows for its windows.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Good Input"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("goodinput.co"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("docs"), QStringLiteral("Open as Docs.")});
    parser.addOption({QStringLiteral("sheets"), QStringLiteral("Open as Sheets.")});
    parser.addOption({QStringLiteral("slides"), QStringLiteral("Open as Slides.")});
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("A file to open."));
    parser.process(app);

    QString kind = QStringLiteral("text");
    QString name = QStringLiteral("Docs");
    if (parser.isSet(QStringLiteral("sheets"))) {
        kind = QStringLiteral("spreadsheet");
        name = QStringLiteral("Sheets");
    } else if (parser.isSet(QStringLiteral("slides"))) {
        kind = QStringLiteral("presentation");
        name = QStringLiteral("Slides");
    }
    QCoreApplication::setApplicationName(name);
    QGuiApplication::setApplicationDisplayName(name);
    QGuiApplication::setDesktopFileName(QStringLiteral("co.goodinput.") + name);

    QUrl file;
    if (!parser.positionalArguments().isEmpty()) {
        file = QUrl::fromUserInput(parser.positionalArguments().constFirst(), QDir::currentPath(), QUrl::AssumeLocalFile);
    }

    auto qml = std::make_unique<QQmlApplicationEngine>();
    QQmlApplicationEngine &engine = *qml;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("kind"), kind}, {QStringLiteral("startFile"), file}});
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("co.goodinput.office", "DocsWindow");
#else
    engine.load(QUrl(QStringLiteral("qrc:/co/goodinput/office/DocsWindow.qml")));
#endif
    // For checks without a screen: draw the window into a picture and quit.
    const QString snapshot = qEnvironmentVariable("GOODINPUT_OFFICE_SNAPSHOT");
    if (!snapshot.isEmpty() && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const int after = qEnvironmentVariableIntValue("GOODINPUT_OFFICE_SNAPSHOT_AFTER");
        QTimer::singleShot(after > 0 ? after : 4000, &app, [window, snapshot] {
            window->grabWindow().save(snapshot);
            QCoreApplication::quit();
        });
    }
    const int code = app.exec();
    // The windows and their documents go first, then the engine, so the
    // engine never stops with a document still open in it.
    qml.reset();
    Engine::instance()->shutdown();
    // LibreOffice's own clean-up at exit runs after its office is gone and
    // crashes; everything is saved and closed by now, so leave at once.
    std::fflush(nullptr);
    std::_Exit(code);
}
