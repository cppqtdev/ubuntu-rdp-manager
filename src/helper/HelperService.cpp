#include "HelperService.h"

#include "HelperProtocol.h"
#include "PackageJob.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QUuid>

#include <pwd.h>

namespace urdm::helper {

namespace p = urdm::protocol;

namespace {
QString userNameFor(unsigned uid)
{
    if (struct passwd *pw = ::getpwuid(uid))
        return QString::fromUtf8(pw->pw_name);
    return QString::number(uid);
}
} // namespace

HelperService::HelperService(HelperCore &core, Authorizer &authorizer, AuditLog &audit, int idleExitSeconds, QObject *parent)
    : QObject(parent)
    , m_core(core)
    , m_auth(authorizer)
    , m_audit(audit)
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(idleExitSeconds * 1000);
    connect(&m_idle, &QTimer::timeout, this, [this]() {
        if (m_inflight == 0 && !m_job)
            QCoreApplication::quit(); // D-Bus activation restarts the helper on the next call
        else
            m_idle.start();
    });
    if (idleExitSeconds > 0)
        m_idle.start();
}

void HelperService::activity(int delta)
{
    m_inflight += delta;
    if (m_idle.interval() > 0)
        m_idle.start();
}

void HelperService::authorizeThen(const QString &actionId, const QString &opName, const QStringList &auditArgs, Operation op)
{
    setDelayedReply(true);
    const QDBusMessage msg = message();
    QDBusConnection bus = connection();
    Caller caller;
    caller.busName = msg.service();
    caller.uid = bus.interface()->serviceUid(caller.busName).value();
    caller.userName = userNameFor(caller.uid);
    activity(+1);

    m_auth.check(caller.busName, actionId, msg.isInteractiveAuthorizationAllowed(),
                 [this, msg, bus, caller, actionId, opName, auditArgs, op](const AuthResult &auth) mutable {
        if (!auth.authorized) {
            const QString why = !auth.error.isEmpty() ? QStringLiteral("authorization check failed: ") + auth.error
                              : auth.challenge       ? QStringLiteral("authentication required but interaction was not allowed")
                                                     : QStringLiteral("not authorized for ") + actionId;
            m_audit.record(caller.uid, caller.userName, opName, auditArgs, QStringLiteral("DENIED"), why);
            bus.send(msg.createErrorReply(p::kErrNotAuthorized, why));
            activity(-1);
            return;
        }
        op(caller, [this, msg, bus, caller, opName, auditArgs](const OpResult &r) mutable {
            m_audit.record(caller.uid, caller.userName, opName, auditArgs, r.ok ? QStringLiteral("OK") : QStringLiteral("FAILED"),
                           r.message);
            if (!r.ok) {
                bus.send(msg.createErrorReply(r.errorName.isEmpty() ? p::kErrFailed : r.errorName, r.message));
            } else if (r.value.metaType() == QMetaType::fromType<QVariantMap>()) {
                bus.send(msg.createReply(QVariant::fromValue(r.value.toMap())));
            } else {
                bus.send(msg.createReply(r.value.isValid() ? r.value.toString() : r.message));
            }
            activity(-1);
        });
    });
}

QString HelperService::Version()
{
    activity(0);
    return QStringLiteral(URDM_VERSION);
}

QString HelperService::ControlService(const QString &action)
{
    authorizeThen(p::kActionServiceControl, QStringLiteral("ControlService"), {action},
                  [this, action](const Caller &, auto reply) { m_core.controlService(action, reply); });
    return {};
}

QString HelperService::ApplyConfigChanges(const QString &file, const QList<DBusConfigChange> &changes)
{
    QStringList args{file};
    QList<ConfigChange> list;
    for (const auto &c : changes) {
        list << ConfigChange{c.section, c.key, c.value, false};
        args << QStringLiteral("[%1]%2=%3").arg(c.section, c.key, c.value);
    }
    authorizeThen(p::kActionConfigModify, QStringLiteral("ApplyConfigChanges"), args,
                  [this, file, list](const Caller &, auto reply) { reply(m_core.applyConfigChanges(file, list)); });
    return {};
}

QString HelperService::RestoreBackup(const QString &file, const QString &backupId)
{
    authorizeThen(p::kActionConfigModify, QStringLiteral("RestoreBackup"), {file, backupId},
                  [this, file, backupId](const Caller &, auto reply) {
                      auto r = m_core.restoreBackup(file, backupId);
                      reply(r);
                  });
    return {};
}

QString HelperService::InstallCertificate(const QString &certPath, const QString &keyPath)
{
    authorizeThen(p::kActionConfigModify, QStringLiteral("InstallCertificate"), {certPath, keyPath},
                  [this, certPath, keyPath](const Caller &caller, auto reply) {
                      auto r = m_core.installCertificate(caller, certPath, keyPath);
                      r.value = {};
                      reply(r);
                  });
    return {};
}

QString HelperService::GenerateSelfSignedCertificate(const QString &commonName, int days)
{
    authorizeThen(p::kActionConfigModify, QStringLiteral("GenerateSelfSignedCertificate"), {commonName, QString::number(days)},
                  [this, commonName, days](const Caller &, auto reply) {
                      auto r = m_core.generateSelfSignedCertificate(commonName, days);
                      r.value = {};
                      reply(r);
                  });
    return {};
}

QString HelperService::ConfigureDesktop(const QString &desktopId)
{
    authorizeThen(p::kActionConfigModify, QStringLiteral("ConfigureDesktop"), {desktopId},
                  [this, desktopId](const Caller &, auto reply) {
                      auto r = m_core.configureDesktop(desktopId);
                      r.value = {};
                      reply(r);
                  });
    return {};
}

QString HelperService::DisconnectSession(const QString &sessionId)
{
    authorizeThen(p::kActionSessionsManage, QStringLiteral("DisconnectSession"), {sessionId},
                  [this, sessionId](const Caller &, auto reply) { reply(m_core.disconnectSession(sessionId)); });
    return {};
}

QString HelperService::TerminateSession(const QString &sessionId)
{
    authorizeThen(p::kActionSessionsManage, QStringLiteral("TerminateSession"), {sessionId},
                  [this, sessionId](const Caller &, auto reply) { reply(m_core.terminateSession(sessionId)); });
    return {};
}

QVariantMap HelperService::GetClientAddresses()
{
    authorizeThen(p::kActionSessionsInspect, QStringLiteral("GetClientAddresses"), {},
                  [this](const Caller &, auto reply) { reply(m_core.clientAddresses()); });
    return {};
}

QString HelperService::SetAccessRestriction(bool enabled, const QString &groupName, const QStringList &initialMembers)
{
    authorizeThen(p::kActionAccessManage, QStringLiteral("SetAccessRestriction"),
                  {enabled ? QStringLiteral("on") : QStringLiteral("off"), groupName, initialMembers.join(QLatin1Char(','))},
                  [this, enabled, groupName, initialMembers](const Caller &, auto reply) {
                      auto r = m_core.setAccessRestriction(enabled, groupName, initialMembers);
                      r.value = {};
                      reply(r);
                  });
    return {};
}

QString HelperService::SetUserAccess(const QString &userName, bool allowed)
{
    authorizeThen(p::kActionAccessManage, QStringLiteral("SetUserAccess"),
                  {userName, allowed ? QStringLiteral("allow") : QStringLiteral("revoke")},
                  [this, userName, allowed](const Caller &, auto reply) { reply(m_core.setUserAccess(userName, allowed)); });
    return {};
}

QString HelperService::StartPackageInstall(const QStringList &components)
{
    authorizeThen(p::kActionPackagesInstall, QStringLiteral("StartPackageInstall"), components,
                  [this, components](const Caller &, auto reply) {
                      if (m_job) {
                          reply(OpResult{false, p::kErrBusy, QStringLiteral("Another installation is already running"), {}});
                          return;
                      }
                      const OpResult plan = m_core.buildInstallPlan(components);
                      if (!plan.ok) {
                          reply(plan);
                          return;
                      }
                      QList<QStringList> commands;
                      for (const QVariant &v : plan.value.toList())
                          commands << v.toStringList();
                      const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                      auto *job = new PackageJob(id, commands, this);
                      m_job = job;
                      connect(job, &PackageJob::output, this, &HelperService::JobOutput);
                      connect(job, &PackageJob::finished, this, [this, job](const QString &jid, bool ok, int code, const QString &summary) {
                          m_audit.record(0, QStringLiteral("helper"), QStringLiteral("PackageJobFinished"), {jid},
                                         ok ? QStringLiteral("OK") : QStringLiteral("FAILED"), summary);
                          emit JobFinished(jid, ok, code, summary);
                          job->deleteLater();
                          activity(0);
                      });
                      reply(OpResult::success(id));
                      // start after the reply so the client knows the job id before output arrives
                      QTimer::singleShot(0, job, &PackageJob::start);
                  });
    return {};
}

QString HelperService::ReadJournal(int maxEntries, qlonglong sinceUnixSeconds)
{
    authorizeThen(p::kActionLogsRead, QStringLiteral("ReadJournal"), {QString::number(maxEntries)},
                  [this, maxEntries, sinceUnixSeconds](const Caller &, auto reply) {
                      reply(m_core.readJournal(maxEntries, sinceUnixSeconds));
                  });
    return {};
}

} // namespace urdm::helper
