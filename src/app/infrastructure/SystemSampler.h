#pragma once

#include "ProcParsers.h"

#include <QHash>
#include <QList>
#include <QString>
#include <optional>

namespace urdm::app {

struct ScopeRef
{
    QString sessionId;
    uint uid = 0;
    QString scope; // session-c1.scope
};

struct ScopeUsage
{
    std::optional<quint64> memoryBytes;
    std::optional<quint64> cpuUsageUsec;
    int processCount = -1;
};

struct ProcessGroupUsage
{
    int count = 0;
    quint64 rssBytes = 0;
    quint64 cpuTicks = 0; // utime + stime
};

// One snapshot of everything the monitor needs. Produced off the GUI thread.
struct SystemSample
{
    qint64 takenMs = 0;
    std::optional<proc::CpuTimes> cpu;
    std::optional<proc::MemInfo> mem;
    std::optional<proc::LoadAvg> load;
    std::optional<double> memPressure;
    std::optional<double> uptimeSeconds;
    QList<int> attachedDisplays;          // displays with an RDP client attached
    QList<proc::TcpSocket> listeners;     // all LISTEN sockets
    QHash<QString, ProcessGroupUsage> processGroups; // by comm: xrdp, xrdp-sesman, Xorg, xrdp-chansrv
    QHash<QString, ScopeUsage> scopes;    // by logind session id
    std::optional<quint64> diskFreeBytes;
    std::optional<quint64> diskTotalBytes;
    int cpuCount = 1;
    long clockTicks = 100;
};

class SystemSampler
{
public:
    struct Roots {
        QString proc = QStringLiteral("/proc");
        QString cgroup = QStringLiteral("/sys/fs/cgroup");
    };
    static SystemSample collect(const QList<ScopeRef> &scopes, const Roots &roots);
    static SystemSample collect(const QList<ScopeRef> &scopes) { return collect(scopes, Roots{}); }
};

} // namespace urdm::app
