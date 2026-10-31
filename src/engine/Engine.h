#pragma once

#include <QObject>
#include <QString>
#include <QThread>

#include <functional>

namespace lok {
class Office;
}

// The one LibreOffice engine in this process. LibreOfficeKit starts once per
// process and is not safe to call from two threads at once, so every call into
// it runs on this engine's own thread, in the order it was asked for.
class Engine : public QObject
{
    Q_OBJECT

public:
    static Engine *instance();

    // Runs the job on the engine thread, after every job asked for before it.
    void run(std::function<void()> job);

    // Runs the job on the engine thread and waits for it. Never call from the
    // engine thread itself.
    void runAndWait(std::function<void()> job);

    // Only valid on the engine thread. Starts the engine on first use.
    lok::Office *office();
    QString startError() const;

    // Where LibreOffice is installed; GOODINPUT_OFFICE_LIBREOFFICE overrides.
    static QString installPath();
    // A profile of our own, so LibreOffice's settings and this program's never meet.
    static QString profileUrl();

    void shutdown();

private:
    Engine();
    ~Engine() override;

    QThread m_thread;
    QObject *m_worker = nullptr;
    lok::Office *m_office = nullptr;
    bool m_started = false;
    QString m_startError;
};
