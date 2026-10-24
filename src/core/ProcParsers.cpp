#include "ProcParsers.h"

#include <QHostAddress>
#include <QRegularExpression>
#include <QtEndian>

#include <cstring>

namespace urdm::proc {

std::optional<CpuTimes> parseCpuTotals(const QByteArray &procStat)
{
    for (const QByteArray &line : procStat.split('\n')) {
        if (!line.startsWith("cpu "))
            continue;
        const QList<QByteArray> f = line.simplified().split(' ');
        if (f.size() < 5)
            return std::nullopt;
        CpuTimes t;
        for (qsizetype i = 1; i < f.size(); ++i) {
            const quint64 v = f.at(i).toULongLong();
            // guest and guest_nice (fields 9,10) are already included in user/nice.
            if (i <= 8)
                t.total += v;
            if (i == 4 || i == 5)
                t.idle += v;
        }
        return t;
    }
    return std::nullopt;
}

double cpuPercent(const CpuTimes &before, const CpuTimes &after)
{
    if (after.total <= before.total)
        return 0.0;
    const double total = double(after.total - before.total);
    const double idle = after.idle >= before.idle ? double(after.idle - before.idle) : 0.0;
    return qBound(0.0, 100.0 * (total - idle) / total, 100.0);
}

std::optional<MemInfo> parseMemInfo(const QByteArray &procMeminfo)
{
    MemInfo m;
    bool haveTotal = false, haveAvail = false;
    for (const QByteArray &line : procMeminfo.split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon);
        const quint64 v = line.mid(colon + 1).trimmed().split(' ').value(0).toULongLong();
        if (key == "MemTotal") { m.totalKiB = v; haveTotal = true; }
        else if (key == "MemAvailable") { m.availableKiB = v; haveAvail = true; }
        else if (key == "SwapTotal") m.swapTotalKiB = v;
        else if (key == "SwapFree") m.swapFreeKiB = v;
    }
    if (!haveTotal || !haveAvail)
        return std::nullopt;
    return m;
}

std::optional<LoadAvg> parseLoadAvg(const QByteArray &procLoadavg)
{
    const QList<QByteArray> f = procLoadavg.simplified().split(' ');
    if (f.size() < 3)
        return std::nullopt;
    bool a, b, c;
    LoadAvg l{f[0].toDouble(&a), f[1].toDouble(&b), f[2].toDouble(&c)};
    if (!a || !b || !c)
        return std::nullopt;
    return l;
}

std::optional<double> parsePressureSomeAvg10(const QByteArray &pressureFile)
{
    static const QRegularExpression re(QStringLiteral(R"(^some\s+avg10=([0-9.]+))"), QRegularExpression::MultilineOption);
    const auto m = re.match(QString::fromLatin1(pressureFile));
    if (!m.hasMatch())
        return std::nullopt;
    return m.captured(1).toDouble();
}

std::optional<double> parseUptimeSeconds(const QByteArray &procUptime)
{
    bool ok = false;
    const double v = procUptime.simplified().split(' ').value(0).toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

namespace {

// /proc/net/tcp stores addresses as hex in host (little-endian) 32-bit word order.
QString decodeAddress(const QByteArray &hex, bool ipv6)
{
    if (!ipv6) {
        // The kernel prints the network-order bytes as a native u32.
        const quint32 native = hex.toUInt(nullptr, 16);
        return QHostAddress(qFromBigEndian(native)).toString();
    }
    if (hex.size() != 32)
        return {};
    Q_IPV6ADDR addr;
    for (int word = 0; word < 4; ++word) {
        // each 32-bit word holds network-order bytes printed as a native u32
        const quint32 w = hex.mid(word * 8, 8).toUInt(nullptr, 16);
        std::memcpy(&addr[word * 4], &w, sizeof w);
    }
    // ::ffff:a.b.c.d (IPv4-mapped) -> a.b.c.d ; "::" stays "::"
    bool mapped = addr[10] == 0xff && addr[11] == 0xff;
    for (int i = 0; i < 10 && mapped; ++i)
        mapped = addr[i] == 0;
    if (mapped)
        return QStringLiteral("%1.%2.%3.%4").arg(addr[12]).arg(addr[13]).arg(addr[14]).arg(addr[15]);
    return QHostAddress(addr).toString();
}

} // namespace

QList<TcpSocket> parseProcNetTcp(const QByteArray &procNetTcp, bool ipv6)
{
    QList<TcpSocket> out;
    const QList<QByteArray> lines = procNetTcp.split('\n');
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> f = lines.at(i).simplified().split(' ');
        if (f.size() < 10)
            continue;
        const QList<QByteArray> local = f.at(1).split(':');
        const QList<QByteArray> remote = f.at(2).split(':');
        if (local.size() != 2 || remote.size() != 2)
            continue;
        TcpSocket s;
        s.ipv6 = ipv6;
        s.localAddress = decodeAddress(local.at(0), ipv6);
        s.localPort = int(local.at(1).toUInt(nullptr, 16));
        s.remoteAddress = decodeAddress(remote.at(0), ipv6);
        s.remotePort = int(remote.at(1).toUInt(nullptr, 16));
        s.state = int(f.at(3).toUInt(nullptr, 16));
        s.inode = f.at(9).toULongLong();
        out << s;
    }
    return out;
}

QList<UnixSocket> parseProcNetUnix(const QByteArray &procNetUnix)
{
    // Num RefCount Protocol Flags Type St Inode [Path]
    QList<UnixSocket> out;
    const QList<QByteArray> lines = procNetUnix.split('\n');
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> f = lines.at(i).simplified().split(' ');
        if (f.size() < 7)
            continue;
        UnixSocket s;
        s.state = int(f.at(5).toUInt(nullptr, 16));
        s.inode = f.at(6).toULongLong();
        if (f.size() >= 8)
            s.path = QString::fromUtf8(f.at(7));
        out << s;
    }
    return out;
}

QList<int> attachedXrdpDisplays(const QList<UnixSocket> &sockets)
{
    static const QRegularExpression re(QStringLiteral(R"(/xrdp_display_(\d+)$)"));
    QList<int> displays;
    for (const auto &s : sockets) {
        if (s.state != kUnixConnected)
            continue;
        const auto m = re.match(s.path);
        if (!m.hasMatch())
            continue;
        const int d = m.captured(1).toInt();
        if (!displays.contains(d))
            displays << d;
    }
    return displays;
}

std::optional<ProcStat> parseProcPidStat(const QByteArray &data)
{
    // pid (comm) state ppid ... ; comm may contain spaces and parentheses -> use last ')'
    const qsizetype open = data.indexOf('(');
    const qsizetype close = data.lastIndexOf(')');
    if (open < 0 || close < open)
        return std::nullopt;
    ProcStat s;
    s.pid = data.left(open).trimmed().toInt();
    s.comm = QString::fromUtf8(data.mid(open + 1, close - open - 1));
    const QList<QByteArray> f = data.mid(close + 1).simplified().split(' ');
    // f[0]=state(3) f[1]=ppid(4) ... utime=14 -> f[11], stime=15 -> f[12], starttime=22 -> f[19], rss=24 -> f[21]
    if (f.size() < 22)
        return std::nullopt;
    s.state = f.at(0).isEmpty() ? '?' : f.at(0).at(0);
    s.ppid = f.at(1).toInt();
    s.utimeTicks = f.at(11).toULongLong();
    s.stimeTicks = f.at(12).toULongLong();
    s.startTimeTicks = f.at(19).toULongLong();
    s.rssPages = f.at(21).toULongLong();
    return s;
}

std::optional<quint64> parseCgroupCpuUsageUsec(const QByteArray &cpuStat)
{
    for (const QByteArray &line : cpuStat.split('\n')) {
        if (line.startsWith("usage_usec ")) {
            bool ok = false;
            const quint64 v = line.mid(11).trimmed().toULongLong(&ok);
            if (ok)
                return v;
        }
    }
    return std::nullopt;
}

} // namespace urdm::proc
