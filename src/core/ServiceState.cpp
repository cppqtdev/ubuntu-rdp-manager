#include "ServiceState.h"

namespace urdm {

ServiceStatus mapServiceStatus(const UnitProperties &p)
{
    if (p.loadState.isEmpty() && p.activeState.isEmpty())
        return ServiceStatus::Unknown;
    if (p.loadState == QLatin1String("not-found"))
        return ServiceStatus::RequiresSetup;
    if (p.loadState == QLatin1String("masked") || p.unitFileState == QLatin1String("masked"))
        return ServiceStatus::Masked;
    if (p.activeState == QLatin1String("active"))
        return ServiceStatus::Running;
    if (p.activeState == QLatin1String("activating"))
        return ServiceStatus::Starting;
    if (p.activeState == QLatin1String("deactivating"))
        return ServiceStatus::Stopping;
    if (p.activeState == QLatin1String("reloading"))
        return ServiceStatus::Reloading;
    if (p.activeState == QLatin1String("failed"))
        return ServiceStatus::Failed;
    if (p.activeState == QLatin1String("inactive"))
        return ServiceStatus::Stopped;
    return ServiceStatus::Unknown;
}

QString serviceStatusText(ServiceStatus s)
{
    switch (s) {
    case ServiceStatus::Unknown: return QStringLiteral("Unknown");
    case ServiceStatus::RequiresSetup: return QStringLiteral("Requires setup");
    case ServiceStatus::Running: return QStringLiteral("Running");
    case ServiceStatus::Stopped: return QStringLiteral("Stopped");
    case ServiceStatus::Starting: return QStringLiteral("Starting");
    case ServiceStatus::Stopping: return QStringLiteral("Stopping");
    case ServiceStatus::Reloading: return QStringLiteral("Reloading");
    case ServiceStatus::Failed: return QStringLiteral("Failed");
    case ServiceStatus::Masked: return QStringLiteral("Masked");
    }
    return {};
}

bool isAutostartEnabled(const UnitProperties &p)
{
    return p.unitFileState == QLatin1String("enabled") || p.unitFileState == QLatin1String("enabled-runtime")
        || p.unitFileState == QLatin1String("alias");
}

} // namespace urdm
