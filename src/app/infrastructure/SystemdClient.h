#pragma once

#include "ServiceState.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QHash>
#include <QObject>
#include <QTimer>

namespace urdm::app {

struct UnitSnapshot
{
    bool valid = false;
    UnitProperties props;
    quint64 activeEnterUsec = 0; // CLOCK_REALTIME usec
    quint32 mainPid = 0;
    QString objectPath;
};

// Read-only, unprivileged view of systemd units. Event-driven via PropertiesChanged
// (after Manager.Subscribe) with a slow safety refresh; never blocks.
class SystemdClient : public QObject
{
    Q_OBJECT
public:
    explicit SystemdClient(QDBusConnection bus, QObject *parent = nullptr);

    void watch(const QString &unit);
    UnitSnapshot unit(const QString &unit) const { return m_units.value(unit); }
    void refreshAll();

signals:
    void unitChanged(const QString &unit);

private slots:
    void onPropertiesChanged(const QDBusMessage &msg);
    void onManagerReloaded();

private:
    void load(const QString &unit);
    void fetch(const QString &unit);

    QDBusConnection m_bus;
    QHash<QString, UnitSnapshot> m_units;
    QHash<QString, QString> m_unitByPath;
    QTimer m_safety;
};

} // namespace urdm::app
