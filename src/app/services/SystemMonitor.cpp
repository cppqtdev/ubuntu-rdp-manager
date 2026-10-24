#include "SystemMonitor.h"

#include <QNetworkInterface>
#include <QSettings>
#include <QtConcurrent>

namespace urdm::app {

namespace {
constexpr int kHistory = 90; // 3 minutes at 2 s

void push(QVariantList &list, double v)
{
    list.append(v);
    while (list.size() > kHistory)
        list.removeFirst();
}
} // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    QSettings st;
    m_thresholds.memAvailableWarnPercent = st.value(QStringLiteral("capacity/memWarnPercent"), m_thresholds.memAvailableWarnPercent).toDouble();
    m_thresholds.loadPerCoreWarn = st.value(QStringLiteral("capacity/loadWarnPerCore"), m_thresholds.loadPerCoreWarn).toDouble();
    m_thresholds.plannedSessionKiB = st.value(QStringLiteral("capacity/plannedSessionKiB"), m_thresholds.plannedSessionKiB).toULongLong();
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::sampleNow);
    connect(&m_watcher, &QFutureWatcher<SystemSample>::finished, this, &SystemMonitor::onSampleReady);
    m_timer.start();
    updateInterfaces();
    sampleNow();
}

void SystemMonitor::setIntervalMs(int ms)
{
    ms = qBound(1000, ms, 60000);
    if (ms == m_timer.interval())
        return;
    m_timer.setInterval(ms);
    emit intervalChanged();
}

void SystemMonitor::setMemWarnPercent(double v)
{
    m_thresholds.memAvailableWarnPercent = qBound(1.0, v, 90.0);
    m_thresholds.memAvailableCritPercent = std::min(m_thresholds.memAvailableCritPercent, m_thresholds.memAvailableWarnPercent / 2);
    saveThresholds();
}

void SystemMonitor::setLoadWarnPerCore(double v)
{
    m_thresholds.loadPerCoreWarn = qBound(0.1, v, 16.0);
    m_thresholds.loadPerCoreCrit = std::max(m_thresholds.loadPerCoreCrit, m_thresholds.loadPerCoreWarn * 1.5);
    saveThresholds();
}

void SystemMonitor::setPlannedSessionMiB(int v)
{
    m_thresholds.plannedSessionKiB = quint64(qBound(64, v, 65536)) * 1024;
    saveThresholds();
}

void SystemMonitor::saveThresholds()
{
    QSettings st;
    st.setValue(QStringLiteral("capacity/memWarnPercent"), m_thresholds.memAvailableWarnPercent);
    st.setValue(QStringLiteral("capacity/loadWarnPerCore"), m_thresholds.loadPerCoreWarn);
    st.setValue(QStringLiteral("capacity/plannedSessionKiB"), m_thresholds.plannedSessionKiB);
    emit thresholdsChanged();
    sampleNow();
}

void SystemMonitor::sampleNow()
{
    if (m_watcher.isRunning())
        return; // never queue up work if the system is slow
    const QList<ScopeRef> scopes = m_scopes;
    m_watcher.setFuture(QtConcurrent::run([scopes]() { return SystemSampler::collect(scopes); }));
}

void SystemMonitor::onSampleReady()
{
    m_prev = m_last;
    m_last = m_watcher.result();
    const bool havePrev = m_hasData;
    m_hasData = true;

    if (havePrev && m_prev.cpu && m_last.cpu)
        m_cpuPercent = proc::cpuPercent(*m_prev.cpu, *m_last.cpu);

    const double elapsedSec = havePrev ? double(m_last.takenMs - m_prev.takenMs) / 1000.0 : 0;
    // xrdp-related CPU: tick delta over wall time, normalised to whole machine
    if (havePrev && elapsedSec > 0) {
        quint64 now = 0, before = 0;
        for (auto it = m_last.processGroups.cbegin(); it != m_last.processGroups.cend(); ++it)
            now += it->cpuTicks;
        for (auto it = m_prev.processGroups.cbegin(); it != m_prev.processGroups.cend(); ++it)
            before += it->cpuTicks;
        const double ticks = now >= before ? double(now - before) : 0;
        m_xrdpCpuPercent = qBound(0.0, 100.0 * ticks / double(m_last.clockTicks) / elapsedSec / m_last.cpuCount, 100.0);
    }

    quint64 sessionMem = 0;
    bool anyMem = false;
    m_sessionCpu.clear();
    for (auto it = m_last.scopes.cbegin(); it != m_last.scopes.cend(); ++it) {
        if (it->memoryBytes) {
            sessionMem += *it->memoryBytes;
            anyMem = true;
        }
        const auto prev = m_prev.scopes.value(it.key());
        if (havePrev && elapsedSec > 0 && it->cpuUsageUsec && prev.cpuUsageUsec && *it->cpuUsageUsec >= *prev.cpuUsageUsec) {
            const double usec = double(*it->cpuUsageUsec - *prev.cpuUsageUsec);
            m_sessionCpu.insert(it.key(), qBound(0.0, 100.0 * usec / 1e6 / elapsedSec / m_last.cpuCount, 100.0));
        }
    }
    m_sessionMemoryMiB = anyMem ? double(sessionMem) / 1048576.0 : (m_last.scopes.isEmpty() ? 0 : -1);

    push(m_cpuHistory, m_cpuPercent);
    push(m_memHistory, memUsedPercent());
    push(m_sessionHistory, m_sessionCount);

    ResourceAdvisor::Input in;
    if (m_last.mem) {
        in.memTotalKiB = m_last.mem->totalKiB;
        in.memAvailableKiB = m_last.mem->availableKiB;
    }
    in.cpuCount = m_last.cpuCount;
    in.load1 = load1();
    in.memoryPressureAvg10 = m_last.memPressure;
    in.sessionCount = m_sessionCount;
    in.measuredSessionMemoryKiB = anyMem ? sessionMem / 1024 : 0;
    in.diskFreeBytes = m_last.diskFreeBytes;
    m_advice = ResourceAdvisor::evaluate(in, m_thresholds);

    if (++m_interfaceTick % 15 == 0)
        updateInterfaces();
    emit sampled();
    emit updated();
}

double SystemMonitor::memUsedPercent() const
{
    if (!m_last.mem || m_last.mem->totalKiB == 0)
        return 0;
    return 100.0 * double(m_last.mem->totalKiB - m_last.mem->availableKiB) / double(m_last.mem->totalKiB);
}

double SystemMonitor::swapUsedMiB() const
{
    if (!m_last.mem)
        return 0;
    return double(m_last.mem->swapTotalKiB - std::min(m_last.mem->swapFreeKiB, m_last.mem->swapTotalKiB)) / 1024.0;
}

QString SystemMonitor::systemUptimeText() const
{
    if (!m_last.uptimeSeconds)
        return QStringLiteral("—");
    const qint64 s = qint64(*m_last.uptimeSeconds);
    return QStringLiteral("%1d %2h %3m").arg(s / 86400).arg((s % 86400) / 3600).arg((s % 3600) / 60);
}

int SystemMonitor::xrdpProcessCount() const
{
    int n = 0;
    for (const auto &g : m_last.processGroups)
        n += g.count;
    return n;
}

double SystemMonitor::xrdpMemoryMiB() const
{
    quint64 b = 0;
    for (const auto &g : m_last.processGroups)
        b += g.rssBytes;
    return double(b) / 1048576.0;
}

QString SystemMonitor::advisorLevel() const
{
    switch (m_advice.level) {
    case ResourceAdvisor::Level::Ok: return QStringLiteral("ok");
    case ResourceAdvisor::Level::Warning: return QStringLiteral("warning");
    case ResourceAdvisor::Level::Critical: return QStringLiteral("critical");
    }
    return QStringLiteral("ok");
}

void SystemMonitor::updateInterfaces()
{
    QVariantList list;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        QStringList addrs;
        for (const auto &e : iface.addressEntries())
            addrs << QStringLiteral("%1/%2").arg(e.ip().toString().section(QLatin1Char('%'), 0, 0)).arg(e.prefixLength());
        list << QVariantMap{{QStringLiteral("name"), iface.humanReadableName()},
                            {QStringLiteral("up"), bool(iface.flags() & QNetworkInterface::IsUp)},
                            {QStringLiteral("loopback"), bool(iface.flags() & QNetworkInterface::IsLoopBack)},
                            {QStringLiteral("addresses"), addrs}};
    }
    m_interfaces = list;
}

} // namespace urdm::app
