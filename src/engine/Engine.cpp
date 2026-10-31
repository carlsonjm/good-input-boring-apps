#define LOK_USE_UNSTABLE_API
#include "Engine.h"

#include <LibreOfficeKit/LibreOfficeKit.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSemaphore>
#include <QStandardPaths>
#include <QUrl>

Engine *Engine::instance()
{
    static Engine *engine = new Engine;
    return engine;
}

Engine::Engine()
{
    m_thread.setObjectName(QStringLiteral("office-engine"));
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    m_thread.start();
}

Engine::~Engine()
{
    shutdown();
}

void Engine::shutdown()
{
    if (!m_thread.isRunning()) {
        return;
    }
    // LibreOffice tears its own threads down when its office object goes.
    runAndWait([this] {
        delete m_office;
        m_office = nullptr;
    });
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    m_worker = nullptr;
}

void Engine::run(std::function<void()> job)
{
    if (!m_worker) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, std::move(job), Qt::QueuedConnection);
}

void Engine::runAndWait(std::function<void()> job)
{
    if (!m_worker) {
        return;
    }
    if (QThread::currentThread() == &m_thread) {
        job();
        return;
    }
    QSemaphore done;
    QMetaObject::invokeMethod(m_worker, [&] {
        job();
        done.release();
    }, Qt::QueuedConnection);
    done.acquire();
}

QString Engine::installPath()
{
    const QByteArray chosen = qgetenv("GOODINPUT_OFFICE_LIBREOFFICE");
    if (!chosen.isEmpty()) {
        return QString::fromLocal8Bit(chosen);
    }
    const QStringList candidates{
        QStringLiteral("/usr/lib/libreoffice/program"),
        QStringLiteral("/usr/lib64/libreoffice/program"),
        QStringLiteral("/opt/libreoffice/program"),
        QStringLiteral("/app/libreoffice/program"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path + QStringLiteral("/libmergedlo.so"))
            || QFileInfo::exists(path + QStringLiteral("/libsofficeapp.so"))) {
            return path;
        }
    }
    return candidates.first();
}

QString Engine::profileUrl()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    const QString dir = base + QStringLiteral("/good-input-office/engine-profile");
    QDir().mkpath(dir + QStringLiteral("/user"));
    // Settings the engine starts with: no marks at the text area's corners,
    // since paper has none. Added to the engine's own settings file if they are
    // not there yet; the engine keeps them from then on.
    const QString settings = dir + QStringLiteral("/user/registrymodifications.xcu");
    QFile file(settings);
    QByteArray current;
    if (file.open(QIODevice::ReadOnly)) {
        current = file.readAll();
        file.close();
    }
    if (!current.contains("DocBoundaries")) {
        QByteArray items;
        for (const char *scheme : {"COLOR_SCHEME_LIBREOFFICE_AUTOMATIC", "COLOR_SCHEME_LIBREOFFICE_LIGHT",
                                   "COLOR_SCHEME_LIBREOFFICE_DARK"}) {
            items += QByteArray("<item oor:path=\"/org.openoffice.Office.UI/ColorScheme/ColorSchemes/"
                                "org.openoffice.Office.UI:ColorScheme['") + scheme
                + "']/DocBoundaries\"><prop oor:name=\"IsVisible\" oor:op=\"fuse\"><value>false</value></prop></item>\n";
        }
        if (current.contains("</oor:items>")) {
            current.replace("</oor:items>", items + "</oor:items>");
        } else {
            current = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<oor:items xmlns:oor=\"http://openoffice.org/2001/registry\" "
                      "xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
                + items + "</oor:items>\n";
        }
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(current);
        }
    }
    return QUrl::fromLocalFile(dir).toString();
}

lok::Office *Engine::office()
{
    Q_ASSERT(QThread::currentThread() == &m_thread);
    if (!m_started) {
        m_started = true;
        const QByteArray path = installPath().toLocal8Bit();
        const QByteArray profile = profileUrl().toUtf8();
        m_office = lok::lok_cpp_init(path.constData(), profile.constData());
        if (!m_office) {
            m_startError = QStringLiteral("LibreOffice could not start from %1").arg(installPath());
        } else {
            // No password feature: without a handler, LibreOffice refuses a locked
            // file at once instead of waiting for a password nobody asks for.
            m_office->setOptionalFeatures(LOK_FEATURE_PART_IN_INVALIDATION_CALLBACK
                                          | LOK_FEATURE_NO_TILED_ANNOTATIONS);
        }
    }
    return m_office;
}

QString Engine::startError() const
{
    return m_startError;
}
