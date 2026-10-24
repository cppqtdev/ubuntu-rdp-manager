#pragma once

#include "AuditLog.h"
#include "Authorizer.h"
#include "DBusTypes.h"
#include "HelperCore.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

namespace urdm::helper {

class PackageJob;

// D-Bus adaptor for com.urdm.Helper1. Every privileged method:
//   1. defers its reply, 2. asks polkit about the caller's unique bus name,
//   3. runs the validated HelperCore operation, 4. writes an audit record, 5. replies.
// There is no generic "run command" or "write file" method.
class HelperService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.urdm.Helper1")

public:
    HelperService(HelperCore &core, Authorizer &authorizer, AuditLog &audit, int idleExitSeconds, QObject *parent = nullptr);

public slots:
    QString Version();
    QString ControlService(const QString &action);
    QString ApplyConfigChanges(const QString &file, const QList<urdm::DBusConfigChange> &changes);
    QString RestoreBackup(const QString &file, const QString &backupId);
    QString InstallCertificate(const QString &certPath, const QString &keyPath);
    QString GenerateSelfSignedCertificate(const QString &commonName, int days);
    QString ConfigureDesktop(const QString &desktopId);
    QString DisconnectSession(const QString &sessionId);
    QString TerminateSession(const QString &sessionId);
    QVariantMap GetClientAddresses();
    QString SetAccessRestriction(bool enabled, const QString &groupName, const QStringList &initialMembers);
    QString SetUserAccess(const QString &userName, bool allowed);
    QString StartPackageInstall(const QStringList &components);
    QString ReadJournal(int maxEntries, qlonglong sinceUnixSeconds);

signals:
    void JobOutput(const QString &jobId, const QString &line);
    void JobFinished(const QString &jobId, bool success, int exitCode, const QString &summary);

private:
    using Operation = std::function<void(const Caller &caller, std::function<void(OpResult)> reply)>;
    void authorizeThen(const QString &actionId, const QString &opName, const QStringList &auditArgs, Operation op);
    void activity(int delta);

    HelperCore &m_core;
    Authorizer &m_auth;
    AuditLog &m_audit;
    QTimer m_idle;
    int m_inflight = 0;
    QPointer<PackageJob> m_job;
};

} // namespace urdm::helper
