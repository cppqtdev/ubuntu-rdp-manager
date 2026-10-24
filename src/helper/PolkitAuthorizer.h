#pragma once

#include "Authorizer.h"

#include <QDBusConnection>
#include <QObject>

namespace urdm::helper {

// Asks polkitd (org.freedesktop.PolicyKit1.Authority.CheckAuthorization) whether the D-Bus
// sender may perform an action. The subject is the caller's unique bus name, which polkit
// resolves to a process/uid itself — no PID-based (racy) subjects.
class PolkitAuthorizer : public QObject, public Authorizer
{
    Q_OBJECT
public:
    explicit PolkitAuthorizer(QDBusConnection bus, QObject *parent = nullptr);
    void check(const QString &busName, const QString &actionId, bool allowInteraction,
               std::function<void(AuthResult)> done) override;

private:
    QDBusConnection m_bus;
};

} // namespace urdm::helper
