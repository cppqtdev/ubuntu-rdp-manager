#pragma once

#include "ConfigStore.h"
#include "SystemOps.h"
#include "XrdpVersion.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <functional>

namespace urdm::helper {

struct HelperPaths
{
    QString xrdpConfigDir = QStringLiteral("/etc/xrdp");
    QString managedCertDir = QStringLiteral("/etc/xrdp/urdm");
    QString backupDir;
    QString startwmDir;     // <prefix>/share/startwm
    QString xrdpSockDir = QStringLiteral("/run/xrdp/sockdir");
    QString xrdpDis = QStringLiteral("/usr/bin/xrdp-dis");
    QString startXfce = QStringLiteral("/usr/bin/startxfce4");
};

struct Caller
{
    QString busName;
    unsigned uid = 0;
    QString userName;
};

struct OpResult
{
    bool ok = false;
    QString errorName;   // protocol::kErr* when !ok
    QString message;
    QVariant value;

    static OpResult success(QVariant v = {}, QString msg = {}) { return {true, {}, std::move(msg), std::move(v)}; }
    static OpResult invalid(QString msg);
    static OpResult failed(QString msg);
};

// All privileged operations. Callers must have been authorized via polkit before any method
// is invoked; HelperCore still validates every argument and never trusts the GUI's view of the
// system (e.g. the access group is read from sesman.ini, not taken from the request).
class HelperCore
{
public:
    HelperCore(SystemOps &ops, HelperPaths paths);

    XrdpVersion xrdpVersion() const;
    ConfigStore &configStore() { return m_store; }

    void controlService(const QString &action, std::function<void(OpResult)> done);
    OpResult applyConfigChanges(const QString &fileName, const QList<ConfigChange> &changes);
    OpResult restoreBackup(const QString &fileName, const QString &backupId);
    OpResult installCertificate(const Caller &caller, const QString &certPath, const QString &keyPath);
    OpResult generateSelfSignedCertificate(const QString &commonName, int days);
    OpResult configureDesktop(const QString &desktopId);
    OpResult disconnectSession(const QString &sessionId);
    OpResult terminateSession(const QString &sessionId);
    OpResult clientAddresses();
    OpResult setAccessRestriction(bool enabled, const QString &groupName, const QStringList &initialMembers);
    OpResult setUserAccess(const QString &userName, bool allowed);
    // Validates the component list and returns the exact command sequence to execute.
    OpResult buildInstallPlan(const QStringList &components);
    OpResult readJournal(int maxEntries, qint64 sinceUnixSeconds);

private:
    OpResult verifiedXrdpSession(const QString &sessionId, int *displayOut);
    bool callerCanRead(const Caller &caller, const FileOwnership &st) const;
    OpResult installCertificateData(const QByteArray &cert, const QByteArray &key);
    std::optional<unsigned> xrdpGroupId();

    SystemOps &m_ops;
    HelperPaths m_paths;
    ConfigStore m_store;
};

} // namespace urdm::helper
