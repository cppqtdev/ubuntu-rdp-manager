#pragma once

#include "ServiceState.h"

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

namespace urdm::app {

class SystemdClient;
class HelperClient;
class SystemMonitor;
class RdpConfigurationManager;

// Server control: real unit state from systemd (event-driven), real listener state from the
// kernel, and authorized actions through the helper.
class RdpServerManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(QString statusKey READ statusKey NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool autostart READ autostart NOTIFY changed)
    Q_PROPERTY(QString activeState READ activeState NOTIFY changed)
    Q_PROPERTY(QString subState READ subState NOTIFY changed)
    Q_PROPERTY(QString unitResult READ unitResult NOTIFY changed)
    Q_PROPERTY(uint mainPid READ mainPid NOTIFY changed)
    Q_PROPERTY(QString sesmanStatusText READ sesmanStatusText NOTIFY changed)
    Q_PROPERTY(qint64 uptimeSeconds READ uptimeSeconds NOTIFY uptimeChanged)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY uptimeChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listenerChanged)
    Q_PROPERTY(QStringList listeningEndpoints READ listeningEndpoints NOTIFY listenerChanged)
    Q_PROPERTY(bool portConflict READ portConflict NOTIFY listenerChanged)
    Q_PROPERTY(QStringList ipAddresses READ ipAddresses NOTIFY ipAddressesChanged)
    Q_PROPERTY(QStringList connectAddresses READ connectAddresses NOTIFY ipAddressesChanged)
    Q_PROPERTY(QString plainStatus READ plainStatus NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    RdpServerManager(SystemdClient *systemd, HelperClient *helper, SystemMonitor *monitor,
                     RdpConfigurationManager *config, QObject *parent = nullptr);

    QString statusKey() const;
    QString statusText() const;
    bool installed() const;
    bool running() const { return m_status == ServiceStatus::Running; }
    bool autostart() const;
    QString activeState() const { return m_xrdp.activeState; }
    QString subState() const { return m_xrdp.subState; }
    QString unitResult() const { return m_xrdp.result; }
    uint mainPid() const { return m_mainPid; }
    QString sesmanStatusText() const;
    qint64 uptimeSeconds() const;
    QString uptimeText() const;
    bool listening() const { return m_listening; }
    QStringList listeningEndpoints() const { return m_listeningEndpoints; }
    bool portConflict() const { return m_portConflict; }
    QStringList ipAddresses() const { return m_ipAddresses; }
    // IPv4 addresses of real network adapters (no loopback, Docker or VPN bridges) to tell users
    QStringList connectAddresses() const { return m_connectAddresses; }
    QString plainStatus() const;
    bool busy() const { return m_busy; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void start() { control(QStringLiteral("start")); }
    Q_INVOKABLE void stop() { control(QStringLiteral("stop")); }
    Q_INVOKABLE void restart() { control(QStringLiteral("restart")); }
    Q_INVOKABLE void setAutostart(bool enabled) { control(enabled ? QStringLiteral("enable") : QStringLiteral("disable")); }
    // One switch for non-technical users: on = start now and at every boot; off = stop and don't start at boot.
    Q_INVOKABLE void setRemoteDesktopEnabled(bool on);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString formatDuration(qint64 seconds) const;

signals:
    void changed();
    void uptimeChanged();
    void listenerChanged();
    void ipAddressesChanged();
    void busyChanged();
    void lastErrorChanged();
    void notification(const QString &level, const QString &text);
    void actionCompleted(const QString &action, bool ok);

private:
    void control(const QString &action, std::function<void(bool)> then = {});
    void onUnitChanged(const QString &unit);
    void updateListener();
    void updateIpAddresses();

    SystemdClient *m_systemd;
    HelperClient *m_helper;
    SystemMonitor *m_monitor;
    RdpConfigurationManager *m_config;
    UnitProperties m_xrdp;
    UnitProperties m_sesman;
    ServiceStatus m_status = ServiceStatus::Unknown;
    quint64 m_activeEnterUsec = 0;
    uint m_mainPid = 0;
    bool m_listening = false;
    bool m_portConflict = false;
    QStringList m_listeningEndpoints;
    QStringList m_ipAddresses;
    QStringList m_connectAddresses;
    bool m_busy = false;
    QString m_lastError;
    QTimer m_uptimeTimer;
    QTimer m_ipTimer;
};

} // namespace urdm::app
