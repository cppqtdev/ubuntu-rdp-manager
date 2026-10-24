#include "ProcessRunner.h"

#include <QCoreApplication>
#include <QPointer>
#include <QProcess>
#include <QTimer>

namespace urdm::app {

namespace {
QList<QPointer<QProcess>> &liveProcesses()
{
    static QList<QPointer<QProcess>> list;
    return list;
}
} // namespace

void ProcessRunner::shutdown()
{
    for (const QPointer<QProcess> &p : std::as_const(liveProcesses())) {
        if (p && p->state() != QProcess::NotRunning) {
            p->disconnect();
            p->kill();
            p->waitForFinished(500);
        }
    }
    liveProcesses().clear();
}

void ProcessRunner::run(QObject *context, const QString &program, const QStringList &args, int timeoutMs,
                        std::function<void(const ProcessResult &)> done)
{
    auto *proc = new QProcess(context);
    auto *timer = new QTimer(proc);
    auto timedOut = std::make_shared<bool>(false);
    auto finishedOnce = std::make_shared<bool>(false);
    QPointer<QObject> guard(context);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("SYSTEMD_COLORS"), QStringLiteral("0"));
    env.insert(QStringLiteral("SYSTEMD_PAGER"), QString());
    proc->setProcessEnvironment(env);
    proc->setStandardInputFile(QProcess::nullDevice());

    auto finish = [proc, done, timedOut, finishedOnce, guard](bool startFailed) {
        if (*finishedOnce)
            return;
        *finishedOnce = true;
        ProcessResult r;
        r.startFailed = startFailed;
        r.timedOut = *timedOut;
        r.stdOut = proc->readAllStandardOutput();
        r.stdErr = proc->readAllStandardError();
        if (!startFailed && !*timedOut)
            r.exitCode = proc->exitStatus() == QProcess::NormalExit ? proc->exitCode() : -1;
        if (startFailed)
            r.stdErr = proc->errorString().toUtf8();
        proc->deleteLater();
        if (guard)
            done(r);
    };

    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, proc, [proc, timedOut]() {
        *timedOut = true;
        proc->kill();
    });
    QObject::connect(proc, &QProcess::finished, proc, [finish]() { finish(false); });
    QObject::connect(proc, &QProcess::errorOccurred, proc, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(true);
    });
    liveProcesses().removeAll(nullptr);
    liveProcesses().append(proc);
    timer->start(timeoutMs);
    proc->start(program, args, QIODevice::ReadOnly);
}

bool ProcessRunner::startDetached(const QString &program, const QStringList &args, QString *error)
{
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.setStandardInputFile(QProcess::nullDevice());
    qint64 pid = 0;
    if (!p.startDetached(&pid)) {
        if (error)
            *error = p.errorString();
        return false;
    }
    return true;
}

} // namespace urdm::app
