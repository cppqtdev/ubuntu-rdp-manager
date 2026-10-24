#include "SystemdClient.h"

#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace urdm::app {

namespace {
const QString kSystemd = QStringLiteral("org.freedesktop.systemd1");
const QString kPath = QStringLiteral("/org/freedesktop/systemd1");
const QString kManager = QStringLiteral("org.freedesktop.systemd1.Manager");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");
}

SystemdClient::SystemdClient(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
    // Subscribe() is permitted for unprivileged clients and enables unit change signals.
    m_bus.asyncCall(QDBusMessage::createMethodCall(kSystemd, kPath, kManager, QStringLiteral("Subscribe")));
    m_bus.connect(kSystemd, kPath, kManager, QStringLiteral("Reloading"), this, SLOT(onManagerReloaded()));
    m_bus.connect(kSystemd, kPath, kManager, QStringLiteral("UnitFilesChanged"), this, SLOT(onManagerReloaded()));
    m_safety.setInterval(15000);
    connect(&m_safety, &QTimer::timeout, this, &SystemdClient::refreshAll);
    m_safety.start();
}

void SystemdClient::watch(const QString &unit)
{
    if (!m_units.contains(unit))
        m_units.insert(unit, {});
    load(unit);
}

void SystemdClient::refreshAll()
{
    for (auto it = m_units.cbegin(); it != m_units.cend(); ++it)
        load(it.key());
}

void SystemdClient::onManagerReloaded()
{
    refreshAll(); // unit files may have appeared after package installation
}

void SystemdClient::load(const QString &unit)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kSystemd, kPath, kManager, QStringLiteral("LoadUnit"));
    msg << unit;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 10000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, unit]() {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> r = *w;
        if (r.isError()) {
            UnitSnapshot s;
            m_units.insert(unit, s);
            emit unitChanged(unit);
            return;
        }
        const QString path = r.value().path();
        UnitSnapshot &s = m_units[unit];
        if (s.objectPath != path) {
            if (!s.objectPath.isEmpty())
                m_bus.disconnect(kSystemd, s.objectPath, kProps, QStringLiteral("PropertiesChanged"), this,
                                 SLOT(onPropertiesChanged(QDBusMessage)));
            s.objectPath = path;
            m_unitByPath.insert(path, unit);
            m_bus.connect(kSystemd, path, kProps, QStringLiteral("PropertiesChanged"), this,
                          SLOT(onPropertiesChanged(QDBusMessage)));
        }
        fetch(unit);
    });
}

void SystemdClient::fetch(const QString &unit)
{
    const QString path = m_units.value(unit).objectPath;
    if (path.isEmpty())
        return;
    auto getAll = [this, path](const QString &iface) {
        QDBusMessage msg = QDBusMessage::createMethodCall(kSystemd, path, kProps, QStringLiteral("GetAll"));
        msg << iface;
        return new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 10000), this);
    };
    auto *wUnit = getAll(QStringLiteral("org.freedesktop.systemd1.Unit"));
    connect(wUnit, &QDBusPendingCallWatcher::finished, this, [this, wUnit, unit, getAll]() {
        wUnit->deleteLater();
        const QDBusPendingReply<QVariantMap> r = *wUnit;
        if (r.isError())
            return;
        const QVariantMap m = r.value();
        UnitSnapshot &s = m_units[unit];
        s.valid = true;
        s.props.loadState = m.value(QStringLiteral("LoadState")).toString();
        s.props.activeState = m.value(QStringLiteral("ActiveState")).toString();
        s.props.subState = m.value(QStringLiteral("SubState")).toString();
        s.props.unitFileState = m.value(QStringLiteral("UnitFileState")).toString();
        s.activeEnterUsec = m.value(QStringLiteral("ActiveEnterTimestamp")).toULongLong();
        if (s.props.loadState != QLatin1String("loaded")) {
            emit unitChanged(unit);
            return;
        }
        auto *wSvc = getAll(QStringLiteral("org.freedesktop.systemd1.Service"));
        connect(wSvc, &QDBusPendingCallWatcher::finished, this, [this, wSvc, unit]() {
            wSvc->deleteLater();
            const QDBusPendingReply<QVariantMap> rs = *wSvc;
            if (!rs.isError()) {
                UnitSnapshot &s2 = m_units[unit];
                s2.mainPid = rs.value().value(QStringLiteral("MainPID")).toUInt();
                s2.props.result = rs.value().value(QStringLiteral("Result")).toString();
            }
            emit unitChanged(unit);
        });
    });
}

void SystemdClient::onPropertiesChanged(const QDBusMessage &msg)
{
    const QString unit = m_unitByPath.value(msg.path());
    if (!unit.isEmpty())
        fetch(unit);
}

} // namespace urdm::app
