#pragma once

#include "ResourceAdvisor.h"
#include "SystemSampler.h"

#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

namespace urdm::app {

// Live resource monitoring. Sampling happens on a worker thread every 2 s (skipped while a
// sample is still running); only derived values are published to QML.
class SystemMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(bool hasData READ hasData NOTIFY updated)
    Q_PROPERTY(double cpuPercent READ cpuPercent NOTIFY updated)
    Q_PROPERTY(int cpuCount READ cpuCount NOTIFY updated)
    Q_PROPERTY(double load1 READ load1 NOTIFY updated)
    Q_PROPERTY(double load5 READ load5 NOTIFY updated)
    Q_PROPERTY(double load15 READ load15 NOTIFY updated)
    Q_PROPERTY(double memTotalMiB READ memTotalMiB NOTIFY updated)
    Q_PROPERTY(double memAvailableMiB READ memAvailableMiB NOTIFY updated)
    Q_PROPERTY(double memUsedPercent READ memUsedPercent NOTIFY updated)
    Q_PROPERTY(double swapUsedMiB READ swapUsedMiB NOTIFY updated)
    Q_PROPERTY(double swapTotalMiB READ swapTotalMiB NOTIFY updated)
    Q_PROPERTY(double memoryPressure READ memoryPressure NOTIFY updated) // -1 = unavailable
    Q_PROPERTY(double diskFreeGiB READ diskFreeGiB NOTIFY updated)
    Q_PROPERTY(double diskTotalGiB READ diskTotalGiB NOTIFY updated)
    Q_PROPERTY(QString systemUptimeText READ systemUptimeText NOTIFY updated)
    Q_PROPERTY(int xrdpProcessCount READ xrdpProcessCount NOTIFY updated)
    Q_PROPERTY(double xrdpMemoryMiB READ xrdpMemoryMiB NOTIFY updated)
    Q_PROPERTY(double xrdpCpuPercent READ xrdpCpuPercent NOTIFY updated)
    Q_PROPERTY(int xorgCount READ xorgCount NOTIFY updated)
    Q_PROPERTY(double sessionMemoryMiB READ sessionMemoryMiB NOTIFY updated)  // -1 = unavailable
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY updated)
    Q_PROPERTY(QVariantList memHistory READ memHistory NOTIFY updated)
    Q_PROPERTY(QVariantList sessionHistory READ sessionHistory NOTIFY updated)
    Q_PROPERTY(QString advisorLevel READ advisorLevel NOTIFY updated)
    Q_PROPERTY(QStringList advisorMessages READ advisorMessages NOTIFY updated)
    Q_PROPERTY(int estimatedAdditionalSessions READ estimatedAdditionalSessions NOTIFY updated)
    Q_PROPERTY(double perSessionMiB READ perSessionMiB NOTIFY updated)
    Q_PROPERTY(bool perSessionMeasured READ perSessionMeasured NOTIFY updated)
    Q_PROPERTY(QVariantList networkInterfaces READ networkInterfaces NOTIFY updated)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalChanged)
    Q_PROPERTY(double memWarnPercent READ memWarnPercent WRITE setMemWarnPercent NOTIFY thresholdsChanged)
    Q_PROPERTY(double loadWarnPerCore READ loadWarnPerCore WRITE setLoadWarnPerCore NOTIFY thresholdsChanged)
    Q_PROPERTY(int plannedSessionMiB READ plannedSessionMiB WRITE setPlannedSessionMiB NOTIFY thresholdsChanged)

public:
    explicit SystemMonitor(QObject *parent = nullptr);

    void setScopes(const QList<ScopeRef> &scopes) { m_scopes = scopes; }
    void setSessionCount(int n) { m_sessionCount = n; }
    void setThresholds(const ResourceAdvisor::Thresholds &t) { m_thresholds = t; }
    const SystemSample &lastSample() const { return m_last; }
    // per-session CPU % since the previous sample (keyed by logind session id)
    double sessionCpuPercent(const QString &sessionId) const { return m_sessionCpu.value(sessionId, -1); }

    bool hasData() const { return m_hasData; }
    double cpuPercent() const { return m_cpuPercent; }
    int cpuCount() const { return m_last.cpuCount; }
    double load1() const { return m_last.load ? m_last.load->one : 0; }
    double load5() const { return m_last.load ? m_last.load->five : 0; }
    double load15() const { return m_last.load ? m_last.load->fifteen : 0; }
    double memTotalMiB() const { return m_last.mem ? double(m_last.mem->totalKiB) / 1024.0 : 0; }
    double memAvailableMiB() const { return m_last.mem ? double(m_last.mem->availableKiB) / 1024.0 : 0; }
    double memUsedPercent() const;
    double swapUsedMiB() const;
    double swapTotalMiB() const { return m_last.mem ? double(m_last.mem->swapTotalKiB) / 1024.0 : 0; }
    double memoryPressure() const { return m_last.memPressure.value_or(-1); }
    double diskFreeGiB() const { return m_last.diskFreeBytes ? double(*m_last.diskFreeBytes) / 1073741824.0 : -1; }
    double diskTotalGiB() const { return m_last.diskTotalBytes ? double(*m_last.diskTotalBytes) / 1073741824.0 : -1; }
    QString systemUptimeText() const;
    int xrdpProcessCount() const;
    double xrdpMemoryMiB() const;
    double xrdpCpuPercent() const { return m_xrdpCpuPercent; }
    int xorgCount() const { return m_last.processGroups.value(QStringLiteral("Xorg")).count; }
    double sessionMemoryMiB() const { return m_sessionMemoryMiB; }
    QVariantList cpuHistory() const { return m_cpuHistory; }
    QVariantList memHistory() const { return m_memHistory; }
    QVariantList sessionHistory() const { return m_sessionHistory; }
    QString advisorLevel() const;
    QStringList advisorMessages() const { return m_advice.messages; }
    int estimatedAdditionalSessions() const { return m_advice.estimatedAdditionalSessions; }
    double perSessionMiB() const { return double(m_advice.perSessionEstimateKiB) / 1024.0; }
    bool perSessionMeasured() const { return m_advice.perSessionMeasured; }
    QVariantList networkInterfaces() const { return m_interfaces; }
    int intervalMs() const { return m_timer.interval(); }
    void setIntervalMs(int ms);

    double memWarnPercent() const { return m_thresholds.memAvailableWarnPercent; }
    void setMemWarnPercent(double v);
    double loadWarnPerCore() const { return m_thresholds.loadPerCoreWarn; }
    void setLoadWarnPerCore(double v);
    int plannedSessionMiB() const { return int(m_thresholds.plannedSessionKiB / 1024); }
    void setPlannedSessionMiB(int v);

    Q_INVOKABLE void sampleNow();

signals:
    void updated();
    void sampled();
    void intervalChanged();
    void thresholdsChanged();

private:
    void onSampleReady();
    void updateInterfaces();
    void saveThresholds();

    QTimer m_timer;
    QFutureWatcher<SystemSample> m_watcher;
    QList<ScopeRef> m_scopes;
    SystemSample m_last;
    SystemSample m_prev;
    bool m_hasData = false;
    double m_cpuPercent = 0;
    double m_xrdpCpuPercent = 0;
    double m_sessionMemoryMiB = -1;
    QHash<QString, double> m_sessionCpu;
    int m_sessionCount = 0;
    QVariantList m_cpuHistory, m_memHistory, m_sessionHistory;
    ResourceAdvisor::Thresholds m_thresholds;
    ResourceAdvisor::Result m_advice;
    QVariantList m_interfaces;
    int m_interfaceTick = 0;
};

} // namespace urdm::app
