#pragma once

#include <QString>

namespace urdm {

// Raw systemd unit properties (org.freedesktop.systemd1.Unit).
struct UnitProperties
{
    QString loadState;     // loaded, not-found, masked, error ...
    QString activeState;   // active, inactive, failed, activating, deactivating, reloading
    QString subState;      // running, dead, failed, start-pre, ...
    QString unitFileState; // enabled, disabled, masked, static ...
    QString result;        // success, exit-code, timeout, ...
};

enum class ServiceStatus { Unknown, RequiresSetup, Running, Stopped, Starting, Stopping, Reloading, Failed, Masked };

ServiceStatus mapServiceStatus(const UnitProperties &p);
QString serviceStatusText(ServiceStatus s);
bool isAutostartEnabled(const UnitProperties &p);

} // namespace urdm
