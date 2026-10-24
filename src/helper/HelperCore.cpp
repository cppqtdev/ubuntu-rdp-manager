#include "HelperCore.h"

#include "HelperProtocol.h"
#include "Validators.h"

#include <QDir>
#include <QHostAddress>
#include <QTemporaryDir>

namespace urdm::helper {

namespace p = urdm::protocol;

OpResult OpResult::invalid(QString msg)
{
    return {false, p::kErrInvalidArgument, std::move(msg), {}};
}

OpResult OpResult::failed(QString msg)
{
    return {false, p::kErrFailed, std::move(msg), {}};
}

QString CommandResult::errorText() const
{
    if (startFailed)
        return QStringLiteral("could not be started");
    if (timedOut)
        return QStringLiteral("timed out");
    const QString err = QString::fromUtf8(stdErr).trimmed();
    return QStringLiteral("exit code %1%2").arg(exitCode).arg(err.isEmpty() ? QString() : QStringLiteral(": ") + err.left(500));
}

HelperCore::HelperCore(SystemOps &ops, HelperPaths paths)
    : m_ops(ops)
    , m_paths(std::move(paths))
    , m_store(m_paths.xrdpConfigDir, m_paths.backupDir)
{
}

XrdpVersion HelperCore::xrdpVersion() const
{
    return XrdpVersion::fromDebianVersion(m_ops.packageVersion(QStringLiteral("xrdp")));
}

// ---------------------------------------------------------------------------------------------
// Service control
// ---------------------------------------------------------------------------------------------
void HelperCore::controlService(const QString &action, std::function<void(OpResult)> done)
{
    if (!p::kServiceActions.contains(action))
        return done(OpResult::invalid(QStringLiteral("Unknown service action \"%1\"").arg(action)));

    const QString load = m_ops.unitLoadState(p::kXrdpUnit);
    if (load != QLatin1String("loaded"))
        return done(OpResult::failed(QStringLiteral("%1 is not installed (LoadState=%2)").arg(p::kXrdpUnit, load.isEmpty() ? QStringLiteral("unknown") : load)));

    if (action == QLatin1String("enable") || action == QLatin1String("disable")) {
        QString err;
        // Both unit files, so a stray WantedBy= on sesman never starts it alone at boot.
        if (!m_ops.setUnitFilesEnabled({p::kXrdpUnit, p::kSesmanUnit}, action == QLatin1String("enable"), &err))
            return done(OpResult::failed(err));
        return done(OpResult::success({}, action == QLatin1String("enable") ? QStringLiteral("Automatic start enabled")
                                                                            : QStringLiteral("Automatic start disabled")));
    }

    // xrdp-sesman.service is BindsTo=xrdp.service, so xrdp.service is the single control point.
    const QString method = action == QLatin1String("start") ? QStringLiteral("StartUnit")
                         : action == QLatin1String("stop")  ? QStringLiteral("StopUnit")
                                                            : QStringLiteral("RestartUnit");
    m_ops.unitJob(method, p::kXrdpUnit, [done](bool ok, const QString &message) {
        done(ok ? OpResult::success({}, message) : OpResult::failed(message));
    });
}

// ---------------------------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------------------------
OpResult HelperCore::applyConfigChanges(const QString &fileName, const QList<ConfigChange> &changes)
{
    const auto file = configFileFromName(fileName);
    if (!file)
        return OpResult::invalid(QStringLiteral("Unknown configuration file \"%1\"").arg(fileName));
    if (changes.size() > 64)
        return OpResult::invalid(QStringLiteral("Too many changes in one request"));
    QString err;
    const auto backup = m_store.write(*file, changes, xrdpVersion(), /*allowInternal*/ false, &err);
    if (!backup)
        return err.contains(QLatin1String("Cannot")) ? OpResult::failed(err) : OpResult::invalid(err);
    return OpResult::success(*backup, QStringLiteral("Saved; previous version backed up as %1").arg(*backup));
}

OpResult HelperCore::restoreBackup(const QString &fileName, const QString &backupId)
{
    const auto file = configFileFromName(fileName);
    if (!file)
        return OpResult::invalid(QStringLiteral("Unknown configuration file \"%1\"").arg(fileName));
    QString err;
    if (!m_store.restore(*file, backupId, &err))
        return OpResult::invalid(err);
    return OpResult::success({}, QStringLiteral("Restored %1").arg(backupId));
}

std::optional<unsigned> HelperCore::xrdpGroupId()
{
    const GroupInfo g = m_ops.group(QStringLiteral("xrdp"));
    if (!g.exists)
        return std::nullopt;
    return g.gid;
}

bool HelperCore::callerCanRead(const Caller &caller, const FileOwnership &st) const
{
    // Confused-deputy protection: the helper only copies files the calling user could read
    // themselves, so it cannot be used to exfiltrate other services' keys.
    if (caller.uid == 0)
        return true;
    if (st.uid == caller.uid)
        return st.mode & 0400;
    if (st.mode & 0004)
        return true;
    if (st.mode & 0040) {
        const auto groups = m_ops.supplementaryGroups(caller.userName);
        return groups.contains(st.gid);
    }
    return false;
}

OpResult HelperCore::installCertificateData(const QByteArray &cert, const QByteArray &key)
{
    const auto xrdpGid = xrdpGroupId();
    if (!xrdpGid)
        return OpResult::failed(QStringLiteral("The xrdp group does not exist; is xrdp installed?"));

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return OpResult::failed(QStringLiteral("Cannot create a temporary directory"));
    const QString certTmp = tmp.filePath(QStringLiteral("cert.pem"));
    const QString keyTmp = tmp.filePath(QStringLiteral("key.pem"));
    QString err;
    if (!m_ops.writeFile(certTmp, cert, 0, 0, 0600, &err) || !m_ops.writeFile(keyTmp, key, 0, 0, 0600, &err))
        return OpResult::failed(err);

    const QString openssl = QStringLiteral("/usr/bin/openssl");
    const auto certPub = m_ops.run(openssl, {QStringLiteral("x509"), QStringLiteral("-in"), certTmp, QStringLiteral("-noout"), QStringLiteral("-pubkey")});
    if (!certPub.ok())
        return OpResult::invalid(QStringLiteral("The certificate is not a valid PEM X.509 certificate"));
    const auto keyPub = m_ops.run(openssl, {QStringLiteral("pkey"), QStringLiteral("-in"), keyTmp, QStringLiteral("-pubout"), QStringLiteral("-passin"), QStringLiteral("pass:")});
    if (!keyPub.ok())
        return OpResult::invalid(QStringLiteral("The private key is not a valid unencrypted PEM key"));
    if (certPub.stdOut.trimmed() != keyPub.stdOut.trimmed())
        return OpResult::invalid(QStringLiteral("The private key does not match the certificate"));
    const auto notExpired = m_ops.run(openssl, {QStringLiteral("x509"), QStringLiteral("-in"), certTmp, QStringLiteral("-noout"), QStringLiteral("-checkend"), QStringLiteral("0")});
    if (!notExpired.ok())
        return OpResult::invalid(QStringLiteral("The certificate has expired"));

    const QString certDst = m_paths.managedCertDir + QStringLiteral("/cert.pem");
    const QString keyDst = m_paths.managedCertDir + QStringLiteral("/key.pem");
    if (!QDir().mkpath(m_paths.managedCertDir))
        return OpResult::failed(QStringLiteral("Cannot create %1").arg(m_paths.managedCertDir));
    // Key: root:xrdp 0640 — readable by the xrdp daemon user, nobody else.
    if (!m_ops.writeFile(certDst, cert, 0, 0, 0644, &err) || !m_ops.writeFile(keyDst, key, 0, *xrdpGid, 0640, &err))
        return OpResult::failed(err);

    const auto backup = m_store.write(ConfigFile::XrdpIni,
                                      {{QStringLiteral("Globals"), QStringLiteral("certificate"), certDst},
                                       {QStringLiteral("Globals"), QStringLiteral("key_file"), keyDst}},
                                      xrdpVersion(), /*allowInternal*/ true, &err);
    if (!backup)
        return OpResult::failed(err);
    return OpResult::success(*backup, QStringLiteral("Certificate installed to %1; restart xrdp to use it").arg(certDst));
}

OpResult HelperCore::installCertificate(const Caller &caller, const QString &certPath, const QString &keyPath)
{
    for (const QString &path : {certPath, keyPath}) {
        if (!validators::isSafeAbsolutePath(path))
            return OpResult::invalid(QStringLiteral("\"%1\" is not an absolute, normalised path").arg(path));
        const FileOwnership st = m_ops.stat(path);
        if (!st.exists)
            return OpResult::invalid(QStringLiteral("%1 does not exist").arg(path));
        if (st.symlink || !st.regularFile)
            return OpResult::invalid(QStringLiteral("%1 must be a regular file (symbolic links are not accepted)").arg(path));
        if (st.size <= 0 || st.size > 64 * 1024)
            return OpResult::invalid(QStringLiteral("%1 has an unexpected size").arg(path));
        if (!callerCanRead(caller, st))
            return OpResult::invalid(QStringLiteral("You do not have read permission for %1").arg(path));
    }
    const auto cert = m_ops.readFile(certPath, 64 * 1024);
    const auto key = m_ops.readFile(keyPath, 64 * 1024);
    if (!cert || !key)
        return OpResult::failed(QStringLiteral("Cannot read the certificate or key file"));
    if (!cert->contains("-----BEGIN CERTIFICATE-----"))
        return OpResult::invalid(QStringLiteral("The certificate file is not PEM encoded"));
    if (!key->contains("PRIVATE KEY-----") || key->contains("ENCRYPTED"))
        return OpResult::invalid(QStringLiteral("The key must be an unencrypted PEM private key (xrdp cannot prompt for a passphrase)"));
    return installCertificateData(*cert, *key);
}

OpResult HelperCore::generateSelfSignedCertificate(const QString &commonName, int days)
{
    if (!validators::isValidHostName(commonName))
        return OpResult::invalid(QStringLiteral("\"%1\" is not a valid host name or IP address").arg(commonName));
    if (days < 1 || days > 825)
        return OpResult::invalid(QStringLiteral("Validity must be between 1 and 825 days"));

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return OpResult::failed(QStringLiteral("Cannot create a temporary directory"));
    const QString keyTmp = tmp.filePath(QStringLiteral("key.pem"));
    const QString certTmp = tmp.filePath(QStringLiteral("cert.pem"));
    const bool isIp = !QHostAddress(commonName).isNull();
    const QString san = (isIp ? QStringLiteral("subjectAltName=IP:") : QStringLiteral("subjectAltName=DNS:")) + commonName;
    const auto r = m_ops.run(QStringLiteral("/usr/bin/openssl"),
                             {QStringLiteral("req"), QStringLiteral("-x509"), QStringLiteral("-newkey"), QStringLiteral("rsa:3072"),
                              QStringLiteral("-nodes"), QStringLiteral("-sha256"), QStringLiteral("-days"), QString::number(days),
                              QStringLiteral("-subj"), QStringLiteral("/CN=") + commonName, QStringLiteral("-addext"), san,
                              QStringLiteral("-addext"), QStringLiteral("extendedKeyUsage=serverAuth"),
                              QStringLiteral("-keyout"), keyTmp, QStringLiteral("-out"), certTmp},
                             {}, 120000);
    if (!r.ok())
        return OpResult::failed(QStringLiteral("openssl failed: %1").arg(r.errorText()));
    const auto cert = m_ops.readFile(certTmp, 64 * 1024);
    const auto key = m_ops.readFile(keyTmp, 64 * 1024);
    if (!cert || !key)
        return OpResult::failed(QStringLiteral("openssl did not produce a certificate"));
    return installCertificateData(*cert, *key);
}

OpResult HelperCore::configureDesktop(const QString &desktopId)
{
    if (!p::kDesktopIds.contains(desktopId))
        return OpResult::invalid(QStringLiteral("Unknown desktop \"%1\"").arg(desktopId));
    QString wm = QStringLiteral("startwm.sh");
    if (desktopId == QLatin1String("xfce")) {
        if (!m_ops.fileExists(m_paths.startXfce))
            return OpResult::failed(QStringLiteral("Xfce is not installed (%1 missing); install the Xfce component first").arg(m_paths.startXfce));
        wm = m_paths.startwmDir + QStringLiteral("/startwm-xfce.sh");
        const FileOwnership st = m_ops.stat(wm);
        // sesman executes this script for every user: it must be root-owned and not writable by others.
        if (!st.exists || !st.regularFile || st.uid != 0 || (st.mode & 0022))
            return OpResult::failed(QStringLiteral("%1 is missing or has unsafe ownership/permissions").arg(wm));
    }
    QString err;
    const auto backup = m_store.write(ConfigFile::SesmanIni, {{QStringLiteral("Globals"), QStringLiteral("DefaultWindowManager"), wm}},
                                      xrdpVersion(), /*allowInternal*/ true, &err);
    if (!backup)
        return OpResult::failed(err);
    return OpResult::success(*backup, QStringLiteral("New sessions will start %1").arg(desktopId == QLatin1String("xfce") ? QStringLiteral("Xfce") : QStringLiteral("the system default desktop")));
}

// ---------------------------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------------------------
OpResult HelperCore::verifiedXrdpSession(const QString &sessionId, int *displayOut)
{
    if (!validators::isValidLogindSessionId(sessionId))
        return OpResult::invalid(QStringLiteral("Invalid session identifier"));
    const auto props = m_ops.sessionProperties(sessionId);
    if (!props)
        return OpResult::invalid(QStringLiteral("Session %1 does not exist").arg(sessionId));
    // Only sessions created by xrdp-sesman may be touched — never local console/SSH logins.
    if (props->value(QStringLiteral("Service")).toString() != p::kSesmanPamService)
        return OpResult::invalid(QStringLiteral("Session %1 is not an xrdp session").arg(sessionId));
    const auto display = validators::parseDisplayNumber(props->value(QStringLiteral("Display")).toString());
    if (!display)
        return OpResult::failed(QStringLiteral("Session %1 has no X11 display").arg(sessionId));
    if (displayOut)
        *displayOut = *display;
    return OpResult::success();
}

OpResult HelperCore::disconnectSession(const QString &sessionId)
{
    int display = -1;
    if (auto v = verifiedXrdpSession(sessionId, &display); !v.ok)
        return v;
    const QString sock = m_paths.xrdpSockDir + QStringLiteral("/xrdp_disconnect_display_%1").arg(display);
    if (!m_ops.fileExists(sock))
        return OpResult::failed(QStringLiteral("Session %1 has no xrdp disconnect socket (already disconnected?)").arg(sessionId));
    // Upstream tool: sends a datagram to xrdp_disconnect_display_<N> derived from $DISPLAY.
    const auto r = m_ops.run(m_paths.xrdpDis, {}, {{QStringLiteral("DISPLAY"), QStringLiteral(":%1").arg(display)}}, 10000);
    if (!r.ok() || !r.stdOut.contains("message sent ok"))
        return OpResult::failed(QStringLiteral("xrdp-dis failed: %1 %2").arg(r.errorText(), QString::fromUtf8(r.stdOut).trimmed()));
    return OpResult::success({}, QStringLiteral("Session %1 (display :%2) disconnected; it keeps running").arg(sessionId).arg(display));
}

OpResult HelperCore::terminateSession(const QString &sessionId)
{
    int display = -1;
    if (auto v = verifiedXrdpSession(sessionId, &display); !v.ok)
        return v;
    QString err;
    if (!m_ops.terminateSession(sessionId, &err))
        return OpResult::failed(err);
    return OpResult::success({}, QStringLiteral("Session %1 (display :%2) terminated").arg(sessionId).arg(display));
}

OpResult HelperCore::clientAddresses()
{
    QString err;
    const auto map = m_ops.xrdpClientAddresses(&err);
    if (!map)
        return OpResult::failed(err);
    return OpResult::success(*map);
}

// ---------------------------------------------------------------------------------------------
// Access control (sesman.ini TerminalServerUsers / AlwaysGroupCheck)
// ---------------------------------------------------------------------------------------------
OpResult HelperCore::setAccessRestriction(bool enabled, const QString &groupName, const QStringList &initialMembers)
{
    QString err;
    if (!enabled) {
        // sesman only enforces TerminalServerUsers when the key names an existing group, so
        // removing the key (and AlwaysGroupCheck) is the documented way to turn checking off.
        const auto backup = m_store.write(ConfigFile::SesmanIni,
                                          {{QStringLiteral("Security"), QStringLiteral("TerminalServerUsers"), {}, true},
                                           {QStringLiteral("Security"), QStringLiteral("AlwaysGroupCheck"), QStringLiteral("false")}},
                                          xrdpVersion(), true, &err);
        if (!backup)
            return OpResult::failed(err);
        return OpResult::success(*backup, QStringLiteral("Every account that passes PAM authentication may now log in"));
    }

    if (!validators::isValidGroupName(groupName))
        return OpResult::invalid(QStringLiteral("\"%1\" is not a valid group name").arg(groupName));
    GroupInfo g = m_ops.group(groupName);
    QStringList members = g.members;
    for (const QString &u : initialMembers) {
        if (!validators::isValidUserName(u) || !m_ops.user(u).exists)
            return OpResult::invalid(QStringLiteral("User \"%1\" does not exist").arg(u));
        if (!members.contains(u))
            members << u;
    }
    if (members.isEmpty())
        return OpResult::invalid(QStringLiteral("Refusing to enable the restriction with an empty group: nobody could log in"));

    if (!g.exists) {
        const auto r = m_ops.run(QStringLiteral("/usr/sbin/groupadd"), {QStringLiteral("--"), groupName});
        if (!r.ok())
            return OpResult::failed(QStringLiteral("groupadd failed: %1").arg(r.errorText()));
    }
    for (const QString &u : initialMembers) {
        if (g.members.contains(u))
            continue;
        const auto r = m_ops.run(QStringLiteral("/usr/bin/gpasswd"), {QStringLiteral("-a"), u, groupName});
        if (!r.ok())
            return OpResult::failed(QStringLiteral("Adding %1 to %2 failed: %3").arg(u, groupName, r.errorText()));
    }
    const auto backup = m_store.write(ConfigFile::SesmanIni,
                                      {{QStringLiteral("Security"), QStringLiteral("TerminalServerUsers"), groupName},
                                       {QStringLiteral("Security"), QStringLiteral("AlwaysGroupCheck"), QStringLiteral("true")}},
                                      xrdpVersion(), true, &err);
    if (!backup)
        return OpResult::failed(err);
    return OpResult::success(*backup, QStringLiteral("Only members of %1 may log in").arg(groupName));
}

OpResult HelperCore::setUserAccess(const QString &userName, bool allowed)
{
    if (!validators::isValidUserName(userName))
        return OpResult::invalid(QStringLiteral("Invalid user name"));
    const UserInfo u = m_ops.user(userName);
    if (!u.exists)
        return OpResult::invalid(QStringLiteral("User \"%1\" does not exist").arg(userName));
    if (u.uid < 1000 || u.uid == 65534)
        return OpResult::invalid(QStringLiteral("%1 is a system account").arg(userName));

    QString err;
    const auto doc = m_store.load(ConfigFile::SesmanIni, &err);
    if (!doc)
        return OpResult::failed(err);
    const QString groupName = doc->value(QStringLiteral("Security"), QStringLiteral("TerminalServerUsers"));
    const GroupInfo g = groupName.isEmpty() ? GroupInfo{} : m_ops.group(groupName);
    if (groupName.isEmpty() || !g.exists)
        return OpResult::invalid(QStringLiteral("Group-based access restriction is not enabled"));
    if (!validators::isValidGroupName(groupName))
        return OpResult::failed(QStringLiteral("sesman.ini names an invalid group"));

    if (allowed == g.members.contains(userName))
        return OpResult::success({}, QStringLiteral("No change"));
    if (!allowed && g.members.size() == 1)
        return OpResult::invalid(QStringLiteral("%1 is the last member of %2; disable the restriction instead of locking everyone out").arg(userName, groupName));
    const auto r = m_ops.run(QStringLiteral("/usr/bin/gpasswd"), {allowed ? QStringLiteral("-a") : QStringLiteral("-d"), userName, groupName});
    if (!r.ok())
        return OpResult::failed(QStringLiteral("gpasswd failed: %1").arg(r.errorText()));
    return OpResult::success({}, allowed ? QStringLiteral("%1 may now use remote desktop").arg(userName)
                                         : QStringLiteral("%1 may no longer start remote desktop sessions (running sessions are not affected)").arg(userName));
}

// ---------------------------------------------------------------------------------------------
// Packages / logs
// ---------------------------------------------------------------------------------------------
OpResult HelperCore::buildInstallPlan(const QStringList &components)
{
    if (components.isEmpty())
        return OpResult::invalid(QStringLiteral("No components selected"));
    QStringList packages;
    for (const QString &c : components) {
        const auto it = p::installComponents().constFind(c);
        if (it == p::installComponents().constEnd())
            return OpResult::invalid(QStringLiteral("Unknown component \"%1\"").arg(c));
        for (const QString &pkg : *it)
            if (!packages.contains(pkg))
                packages << pkg;
    }
    const QString aptGet = QStringLiteral("/usr/bin/apt-get");
    QList<QStringList> plan;
    plan << QStringList{aptGet, QStringLiteral("update")};
    QStringList install{aptGet, QStringLiteral("install"), QStringLiteral("-y"),
                        // keep administrator-modified configuration files on upgrade
                        QStringLiteral("-o"), QStringLiteral("Dpkg::Options::=--force-confdef"),
                        QStringLiteral("-o"), QStringLiteral("Dpkg::Options::=--force-confold"), QStringLiteral("--")};
    install << packages;
    plan << install;
    QVariantList v;
    for (const auto &cmd : plan)
        v << cmd;
    return OpResult::success(v);
}

OpResult HelperCore::readJournal(int maxEntries, qint64 sinceUnixSeconds)
{
    if (maxEntries < 1 || maxEntries > 5000)
        return OpResult::invalid(QStringLiteral("maxEntries must be 1..5000"));
    if (sinceUnixSeconds < 0)
        return OpResult::invalid(QStringLiteral("Invalid start time"));
    QStringList args{QStringLiteral("--no-pager"), QStringLiteral("-o"), QStringLiteral("json"),
                     QStringLiteral("-u"), p::kXrdpUnit, QStringLiteral("-u"), p::kSesmanUnit,
                     QStringLiteral("-n"), QString::number(maxEntries)};
    if (sinceUnixSeconds > 0)
        args << QStringLiteral("--since") << QStringLiteral("@%1").arg(sinceUnixSeconds);
    const auto r = m_ops.run(QStringLiteral("/usr/bin/journalctl"), args, {}, 30000);
    if (!r.ok())
        return OpResult::failed(QStringLiteral("journalctl failed: %1").arg(r.errorText()));
    return OpResult::success(QString::fromUtf8(r.stdOut.left(8 * 1024 * 1024)));
}

} // namespace urdm::helper
