#include "RdpServerManager.h"

#include "HelperClient.h"
#include "HelperProtocol.h"
#include "ListenSpec.h"
#include "RdpConfigurationManager.h"
#include "SystemMonitor.h"
#include "SystemdClient.h"

#include <QDateTime>
#include <algorithm>
#include <QNetworkInterface>

namespace urdm::app {

namespace p = urdm::protocol;

RdpServerManager::RdpServerManager(SystemdClient *systemd, HelperClient *helper, SystemMonitor *monitor,
                                   RdpConfigurationManager *config, QObject *parent)
    : QObject(parent)
    , m_systemd(systemd)
    , m_helper(helper)
    , m_monitor(monitor)
    , m_config(config)
{
    connect(m_systemd, &SystemdClient::unitChanged, this, &RdpServerManager::onUnitChanged);
    m_systemd->watch(p::kXrdpUnit);
    m_systemd->watch(p::kSesmanUnit);
    connect(m_monitor, &SystemMonitor::sampled, this, &RdpServerManager::updateListener);
    connect(m_config, &RdpConfigurationManager::loaded, this, &RdpServerManager::updateListener);

    m_uptimeTimer.setInterval(1000);
    connect(&m_uptimeTimer, &QTimer::timeout, this, &RdpServerManager::uptimeChanged);
    m_uptimeTimer.start();
    m_ipTimer.setInterval(30000); // interfaces change rarely
    connect(&m_ipTimer, &QTimer::timeout, this, &RdpServerManager::updateIpAddresses);
    m_ipTimer.start();
    updateIpAddresses();
}

void RdpServerManager::onUnitChanged(const QString &unit)
{
    const UnitSnapshot s = m_systemd->unit(unit);
    if (unit == p::kXrdpUnit) {
        m_xrdp = s.props;
        m_activeEnterUsec = s.activeEnterUsec;
        m_mainPid = s.mainPid;
        m_status = s.valid ? mapServiceStatus(m_xrdp) : ServiceStatus::Unknown;
    } else if (unit == p::kSesmanUnit) {
        m_sesman = s.props;
    }
    emit changed();
    emit uptimeChanged();
    updateListener();
}

QString RdpServerManager::statusKey() const
{
    switch (m_status) {
    case ServiceStatus::Running: return QStringLiteral("running");
    case ServiceStatus::Stopped: return QStringLiteral("stopped");
    case ServiceStatus::Starting: return QStringLiteral("starting");
    case ServiceStatus::Stopping: return QStringLiteral("stopping");
    case ServiceStatus::Reloading: return QStringLiteral("starting");
    case ServiceStatus::Failed: return QStringLiteral("failed");
    case ServiceStatus::RequiresSetup: return QStringLiteral("requires-setup");
    case ServiceStatus::Masked: return QStringLiteral("failed");
    case ServiceStatus::Unknown: break;
    }
    return QStringLiteral("unknown");
}

QString RdpServerManager::statusText() const
{
    return serviceStatusText(m_status);
}

QString RdpServerManager::plainStatus() const
{
    switch (m_status) {
    case ServiceStatus::Running: return QStringLiteral("on");
    case ServiceStatus::Stopped: return QStringLiteral("off");
    case ServiceStatus::Starting:
    case ServiceStatus::Reloading: return QStringLiteral("starting");
    case ServiceStatus::Stopping: return QStringLiteral("stopping");
    case ServiceStatus::Failed:
    case ServiceStatus::Masked: return QStringLiteral("problem");
    case ServiceStatus::RequiresSetup: return QStringLiteral("not-installed");
    case ServiceStatus::Unknown: break;
    }
    return QStringLiteral("unknown");
}

bool RdpServerManager::installed() const
{
    return m_status != ServiceStatus::RequiresSetup && m_status != ServiceStatus::Unknown;
}

bool RdpServerManager::autostart() const
{
    return isAutostartEnabled(m_xrdp);
}

QString RdpServerManager::sesmanStatusText() const
{
    if (m_sesman.loadState.isEmpty())
        return QStringLiteral("Unknown");
    // Ubuntu's ExecStop (xrdp-sesman --kill) exits 1 when the BindsTo stop already ended the
    // daemon, so a normal stop leaves the unit "failed". Report that truthfully but not as a fault.
    if (m_sesman.activeState == QLatin1String("failed") && m_status == ServiceStatus::Stopped && m_sesman.result == QLatin1String("exit-code"))
        return QStringLiteral("Stopped (stop command returned status 1 — known Ubuntu packaging quirk, harmless)");
    return serviceStatusText(mapServiceStatus(m_sesman));
}

qint64 RdpServerManager::uptimeSeconds() const
{
    if (m_status != ServiceStatus::Running || m_activeEnterUsec == 0)
        return -1;
    return QDateTime::currentMSecsSinceEpoch() / 1000 - qint64(m_activeEnterUsec / 1000000);
}

QString RdpServerManager::formatDuration(qint64 s) const
{
    if (s < 0)
        return QStringLiteral("—");
    const qint64 d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
    if (d > 0)
        return QStringLiteral("%1d %2h %3m").arg(d).arg(h).arg(m);
    if (h > 0)
        return QStringLiteral("%1h %2m").arg(h).arg(m);
    if (m > 0)
        return QStringLiteral("%1m %2s").arg(m).arg(s % 60);
    return QStringLiteral("%1s").arg(s);
}

QString RdpServerManager::uptimeText() const
{
    return formatDuration(uptimeSeconds());
}

void RdpServerManager::updateListener()
{
    const QList<int> ports = m_config->configuredTcpPorts();
    const auto &listeners = m_monitor->lastSample().listeners;
    QStringList endpoints;
    bool any = false;
    for (const auto &l : listeners) {
        if (!ports.contains(l.localPort))
            continue;
        any = true;
        const QString ep = (l.localAddress.contains(QLatin1Char(':')) ? QStringLiteral("[%1]:%2") : QStringLiteral("%1:%2"))
                               .arg(l.localAddress).arg(l.localPort);
        if (!endpoints.contains(ep))
            endpoints << ep;
    }
    // Something else listening on the RDP port while xrdp is not running.
    const bool conflict = any && m_status != ServiceStatus::Running && m_status != ServiceStatus::Starting
                          && m_status != ServiceStatus::Stopping;
    if (any != m_listening || endpoints != m_listeningEndpoints || conflict != m_portConflict) {
        m_listening = any;
        m_listeningEndpoints = endpoints;
        m_portConflict = conflict;
        emit listenerChanged();
    }
}

void RdpServerManager::updateIpAddresses()
{
    static const QStringList virtualPrefixes{QStringLiteral("docker"), QStringLiteral("br-"), QStringLiteral("veth"),
                                             QStringLiteral("virbr"), QStringLiteral("lxc"), QStringLiteral("vmnet"),
                                             QStringLiteral("vboxnet"), QStringLiteral("cni"), QStringLiteral("flannel")};
    QStringList connect;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        if (std::any_of(virtualPrefixes.cbegin(), virtualPrefixes.cend(),
                        [&](const QString &p) { return iface.name().startsWith(p); }))
            continue;
        for (const QNetworkAddressEntry &e : iface.addressEntries())
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol)
                connect << e.ip().toString();
    }
    QStringList ips;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &e : iface.addressEntries()) {
            const QHostAddress a = e.ip();
            if (a.isLinkLocal())
                continue;
            ips << QStringLiteral("%1 (%2)").arg(a.toString().section(QLatin1Char('%'), 0, 0), iface.humanReadableName());
        }
    }
    if (ips != m_ipAddresses || connect != m_connectAddresses) {
        m_ipAddresses = ips;
        m_connectAddresses = connect;
        emit ipAddressesChanged();
    }
}

void RdpServerManager::refresh()
{
    m_systemd->refreshAll();
    updateIpAddresses();
}

void RdpServerManager::setRemoteDesktopEnabled(bool on)
{
    // polkit's auth_admin_keep means the user is asked for the password only once for both steps.
    if (on)
        control(QStringLiteral("enable"), [this](bool ok) { if (ok) control(QStringLiteral("start")); });
    else
        control(QStringLiteral("stop"), [this](bool ok) { if (ok) control(QStringLiteral("disable")); });
}

void RdpServerManager::control(const QString &action, std::function<void(bool)> then)
{
    if (m_busy)
        return;
    m_busy = true;
    emit busyChanged();
    m_helper->call(QStringLiteral("ControlService"), {action}, [this, action, then](const HelperReply &r) {
        m_busy = false;
        emit busyChanged();
        static const QHash<QString, QPair<QString, QString>> words{
            {QStringLiteral("start"), {QStringLiteral("Remote Desktop is now ON. People can connect."), QStringLiteral("turn on")}},
            {QStringLiteral("stop"), {QStringLiteral("Remote Desktop is now OFF. Nobody can connect."), QStringLiteral("turn off")}},
            {QStringLiteral("restart"), {QStringLiteral("Remote Desktop was restarted."), QStringLiteral("restart")}},
            {QStringLiteral("enable"), {QStringLiteral("Remote Desktop will start automatically when this computer starts."), QStringLiteral("set up automatic start for")}},
            {QStringLiteral("disable"), {QStringLiteral("Remote Desktop will no longer start automatically."), QStringLiteral("switch off automatic start for")}},
        };
        const auto w = words.value(action);
        m_lastError = r.ok || r.notAuthorized ? QString()
                                              : QStringLiteral("Could not %1 Remote Desktop. Details: %2").arg(w.second, r.message);
        emit lastErrorChanged();
        // Only report the final step of a combined on/off action.
        if (!r.ok)
            emit notification(r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error"),
                              r.notAuthorized ? r.message : m_lastError);
        else if (!then)
            emit notification(QStringLiteral("success"), w.first);
        m_systemd->refreshAll();
        emit actionCompleted(action, r.ok);
        if (then)
            then(r.ok);
    });
}

} // namespace urdm::app
