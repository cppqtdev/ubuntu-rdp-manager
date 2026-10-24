#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <functional>

namespace urdm::app {

struct ProcessResult
{
    int exitCode = -1;
    QByteArray stdOut;
    QByteArray stdErr;
    bool startFailed = false;
    bool timedOut = false;
    bool ok() const { return !startFailed && !timedOut && exitCode == 0; }
};

// Asynchronous, unprivileged subprocess execution: arguments are passed as a list (never a
// shell string), output is collected, a timeout kills the child, and the callback runs on the
// caller's thread. Never blocks the GUI thread.
class ProcessRunner
{
public:
    static void run(QObject *context, const QString &program, const QStringList &args, int timeoutMs,
                    std::function<void(const ProcessResult &)> done);
    // Kills every child still running. Called on application exit so no child outlives us.
    static void shutdown();
    // Starts a detached interactive program (e.g. an RDP client). Returns false if it cannot start.
    static bool startDetached(const QString &program, const QStringList &args, QString *error);
};

} // namespace urdm::app
