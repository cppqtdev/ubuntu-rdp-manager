#include "SystemSampler.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStorageInfo>
#include <QThread>

#include <unistd.h>

namespace urdm::app {

namespace {
QByteArray readSmall(const QString &path, qint64 max = 4 * 1024 * 1024)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.read(max);
}

const QStringList kTrackedComms{QStringLiteral("xrdp"), QStringLiteral("xrdp-sesman"), QStringLiteral("Xorg"),
                                QStringLiteral("xrdp-chansrv")};
} // namespace

SystemSample SystemSampler::collect(const QList<ScopeRef> &scopes, const Roots &roots)
{
    SystemSample s;
    s.takenMs = QDateTime::currentMSecsSinceEpoch();
    s.cpuCount = std::max(1, QThread::idealThreadCount());
    s.clockTicks = ::sysconf(_SC_CLK_TCK);

    s.cpu = proc::parseCpuTotals(readSmall(roots.proc + QStringLiteral("/stat")));
    s.mem = proc::parseMemInfo(readSmall(roots.proc + QStringLiteral("/meminfo")));
    s.load = proc::parseLoadAvg(readSmall(roots.proc + QStringLiteral("/loadavg")));
    s.memPressure = proc::parsePressureSomeAvg10(readSmall(roots.proc + QStringLiteral("/pressure/memory")));
    s.uptimeSeconds = proc::parseUptimeSeconds(readSmall(roots.proc + QStringLiteral("/uptime")));
    s.attachedDisplays = proc::attachedXrdpDisplays(proc::parseProcNetUnix(readSmall(roots.proc + QStringLiteral("/net/unix"))));

    for (bool v6 : {false, true}) {
        const auto socks = proc::parseProcNetTcp(readSmall(roots.proc + (v6 ? QStringLiteral("/net/tcp6") : QStringLiteral("/net/tcp"))), v6);
        for (const auto &t : socks)
            if (t.state == proc::kTcpListen)
                s.listeners << t;
    }

    // xrdp-related processes (stat/status are world readable)
    const QDir procDir(roots.proc);
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    for (const QString &entry : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        entry.toInt(&isPid);
        if (!isPid)
            continue;
        const auto st = proc::parseProcPidStat(readSmall(roots.proc + QLatin1Char('/') + entry + QStringLiteral("/stat"), 4096));
        if (!st || !kTrackedComms.contains(st->comm))
            continue;
        ProcessGroupUsage &g = s.processGroups[st->comm];
        g.count += 1;
        g.rssBytes += st->rssPages * quint64(pageSize);
        g.cpuTicks += st->utimeTicks + st->stimeTicks;
    }

    // Per-session accounting from the logind session scope (cgroup v2).
    for (const ScopeRef &ref : scopes) {
        if (ref.scope.isEmpty() || ref.scope.contains(QLatin1Char('/')))
            continue;
        const QString dir = QStringLiteral("%1/user.slice/user-%2.slice/%3").arg(roots.cgroup).arg(ref.uid).arg(ref.scope);
        ScopeUsage u;
        bool ok = false;
        const quint64 mem = readSmall(dir + QStringLiteral("/memory.current"), 64).trimmed().toULongLong(&ok);
        if (ok)
            u.memoryBytes = mem;
        u.cpuUsageUsec = proc::parseCgroupCpuUsageUsec(readSmall(dir + QStringLiteral("/cpu.stat"), 4096));
        const QByteArray procs = readSmall(dir + QStringLiteral("/cgroup.procs"), 1024 * 1024);
        if (QFile::exists(dir + QStringLiteral("/cgroup.procs")))
            u.processCount = int(procs.count('\n'));
        s.scopes.insert(ref.sessionId, u);
    }

    const QStorageInfo root(QStringLiteral("/"));
    if (root.isValid() && root.isReady()) {
        s.diskFreeBytes = quint64(root.bytesAvailable());
        s.diskTotalBytes = quint64(root.bytesTotal());
    }
    return s;
}

} // namespace urdm::app
