#include "PackageJob.h"

#include <QProcessEnvironment>

namespace urdm::helper {

PackageJob::PackageJob(QString id, QList<QStringList> plan, QObject *parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_plan(std::move(plan))
{
    QProcessEnvironment env;
    env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin:/sbin:/bin"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("DEBIAN_FRONTEND"), QStringLiteral("noninteractive"));
    env.insert(QStringLiteral("APT_LISTCHANGES_FRONTEND"), QStringLiteral("none"));
    m_proc.setProcessEnvironment(env);
    m_proc.setWorkingDirectory(QStringLiteral("/"));
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    m_proc.setStandardInputFile(QProcess::nullDevice());

    connect(&m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        m_buffer += m_proc.readAllStandardOutput();
        flush(false);
    });
    connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit finished(m_id, false, -1, QStringLiteral("Could not start %1").arg(m_plan.value(m_step).value(0)));
    });
    connect(&m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        flush(true);
        const QString cmd = m_plan.value(m_step).mid(0, 2).join(QLatin1Char(' '));
        if (status != QProcess::NormalExit || code != 0) {
            emit finished(m_id, false, status == QProcess::NormalExit ? code : -1,
                          QStringLiteral("\"%1\" failed (exit code %2)").arg(cmd).arg(code));
            return;
        }
        ++m_step;
        runNext();
    });
}

void PackageJob::start()
{
    runNext();
}

void PackageJob::runNext()
{
    if (m_step >= m_plan.size()) {
        emit finished(m_id, true, 0, QStringLiteral("Installation completed"));
        return;
    }
    const QStringList cmd = m_plan.at(m_step);
    emit output(m_id, QStringLiteral("$ ") + cmd.join(QLatin1Char(' ')));
    m_proc.start(cmd.first(), cmd.mid(1));
}

void PackageJob::flush(bool final)
{
    qsizetype nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (!line.isEmpty())
            emit output(m_id, QString::fromUtf8(line));
    }
    if (final && !m_buffer.isEmpty()) {
        emit output(m_id, QString::fromUtf8(m_buffer));
        m_buffer.clear();
    }
}

} // namespace urdm::helper
