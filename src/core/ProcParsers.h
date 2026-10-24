#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <optional>

// Pure parsers for Linux /proc and cgroup v2 text formats. They never touch the filesystem,
// which keeps them unit-testable with captured fixtures.
namespace urdm::proc {

struct CpuTimes {
    quint64 total = 0;
    quint64 idle = 0; // idle + iowait
};
std::optional<CpuTimes> parseCpuTotals(const QByteArray &procStat);
// Utilisation in percent between two samples (0..100).
double cpuPercent(const CpuTimes &before, const CpuTimes &after);

struct MemInfo {
    quint64 totalKiB = 0;
    quint64 availableKiB = 0;
    quint64 swapTotalKiB = 0;
    quint64 swapFreeKiB = 0;
};
std::optional<MemInfo> parseMemInfo(const QByteArray &procMeminfo);

struct LoadAvg {
    double one = 0, five = 0, fifteen = 0;
};
std::optional<LoadAvg> parseLoadAvg(const QByteArray &procLoadavg);

// /proc/pressure/memory "some avg10=..." value (percent), when PSI is available.
std::optional<double> parsePressureSomeAvg10(const QByteArray &pressureFile);

std::optional<double> parseUptimeSeconds(const QByteArray &procUptime);

struct TcpSocket {
    QString localAddress; // textual IP
    int localPort = 0;
    QString remoteAddress;
    int remotePort = 0;
    int state = 0;        // kernel TCP state (0x0A = LISTEN, 0x01 = ESTABLISHED)
    quint64 inode = 0;
    bool ipv6 = false;
};
QList<TcpSocket> parseProcNetTcp(const QByteArray &procNetTcp, bool ipv6);
constexpr int kTcpEstablished = 0x01;
constexpr int kTcpListen = 0x0A;

struct UnixSocket {
    quint64 inode = 0;
    int state = 0;        // 0x01 SS_UNCONNECTED, 0x03 SS_CONNECTED
    QString path;
};
QList<UnixSocket> parseProcNetUnix(const QByteArray &procNetUnix);
constexpr int kUnixConnected = 0x03;

// Display numbers whose xrdp display socket (…/xrdp_display_N) has a connected peer, i.e.
// an RDP client is currently attached (verified behaviour, docs/compatibility.md).
QList<int> attachedXrdpDisplays(const QList<UnixSocket> &sockets);

struct ProcStat {
    int pid = 0;
    QString comm;
    char state = '?';
    int ppid = 0;
    quint64 utimeTicks = 0;
    quint64 stimeTicks = 0;
    quint64 startTimeTicks = 0;
    quint64 rssPages = 0;
};
std::optional<ProcStat> parseProcPidStat(const QByteArray &procPidStat);

// cgroup v2 "cpu.stat" usage_usec field.
std::optional<quint64> parseCgroupCpuUsageUsec(const QByteArray &cpuStat);

} // namespace urdm::proc
