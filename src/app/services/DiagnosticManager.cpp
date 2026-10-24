#include "DiagnosticManager.h"

#include "HelperClient.h"
#include "LogManager.h"
#include "LogSanitizer.h"
#include "ProcessRunner.h"
#include "RdpConfigurationManager.h"
#include "RdpProbe.h"
#include "RdpServerManager.h"
#include "SessionManager.h"
#include "SystemMonitor.h"
#include "UserAccessManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSettings>
#include <QSslCertificate>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QUrl>

#include <grp.h>
#include <sys/stat.h>

namespace urdm::app {

namespace {
// "%n thing(s)" with correct singular/plural ("1 person", "3 people")
QString counted(const char *text, qint64 n)
{
    QString t = QString::fromUtf8(text);
    t.replace(QLatin1String("%n"), QString::number(n));
    t.replace(QLatin1String("person(s)"), n == 1 ? QStringLiteral("person") : QStringLiteral("people"));
    t.replace(QLatin1String("(s)"), n == 1 ? QString() : QStringLiteral("s"));
    return t;
}

QStringList groupMembers(const char *name, bool *exists = nullptr, unsigned *gid = nullptr)
{
    QStringList m;
    if (struct group *g = ::getgrnam(name)) {
        if (exists)
            *exists = true;
        if (gid)
            *gid = g->gr_gid;
        for (char **p = g->gr_mem; p && *p; ++p)
            m << QString::fromUtf8(*p);
    }
    return m;
}
} // namespace

DiagnosticManager::DiagnosticManager(RdpConfigurationManager *config, RdpServerManager *server, SystemMonitor *monitor,
                                     LogManager *logs, SessionManager *sessions, UserAccessManager *users, HelperClient *helper,
                                     QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_server(server)
    , m_monitor(monitor)
    , m_logs(logs)
    , m_sessions(sessions)
    , m_users(users)
    , m_helper(helper)
    , m_probe(new RdpProbe(this))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(500);
    connect(&m_debounce, &QTimer::timeout, this, &DiagnosticManager::runChecks);
    auto schedule = [this]() { m_debounce.start(); };
    connect(m_config, &RdpConfigurationManager::loaded, this, schedule);
    connect(m_server, &RdpServerManager::changed, this, schedule);
    connect(m_server, &RdpServerManager::listenerChanged, this, schedule);
    connect(m_logs, &LogManager::countersChanged, this, schedule);
    connect(m_users, &UserAccessManager::changed, this, schedule);
    connect(m_helper, &HelperClient::availableChanged, this, schedule);
    QTimer *slow = new QTimer(this); // resource checks drift slowly
    slow->setInterval(30000);
    connect(slow, &QTimer::timeout, this, schedule);
    slow->start();

    connect(m_probe, &RdpProbe::finished, this, [this](const QVariantList &stages, bool net, bool rdp, bool tls) {
        m_stages = stages;
        m_summary = !net ? QStringLiteral("Could not reach the computer. Check the address, the port, and that Remote Desktop is ON.")
                  : !rdp ? QStringLiteral("Something answers on this port, but it is not a remote desktop server.")
                  : !tls ? QStringLiteral("The remote desktop server answers, but a safe (encrypted) connection could not be made.")
                         : QStringLiteral("Everything looks good: the computer can be reached and the connection is encrypted. "
                                          "To finish, sign in from another computer (or press \"Try it\").");
        m_currentRecord.insert(QStringLiteral("stages"), stages);
        m_currentRecord.insert(QStringLiteral("summary"), m_summary);
        m_history.prepend(m_currentRecord);
        while (m_history.size() > 20)
            m_history.removeLast();
        saveHistory();
        emit testChanged();
    });
    connect(slow, &QTimer::timeout, this, &DiagnosticManager::refreshGnomeRemoteDesktop);
    refreshGnomeRemoteDesktop();
    m_history = QSettings().value(QStringLiteral("diagnostics/testHistory")).toList();
    detectClient();
    m_debounce.start();
}

int DiagnosticManager::failCount() const
{
    int n = 0;
    for (const auto &c : m_checks)
        n += c.toMap().value(QStringLiteral("status")) == QLatin1String("fail");
    return n;
}

int DiagnosticManager::warnCount() const
{
    int n = 0;
    for (const auto &c : m_checks)
        n += c.toMap().value(QStringLiteral("status")) == QLatin1String("warn");
    return n;
}

bool DiagnosticManager::testRunning() const
{
    return m_probe->running();
}

void DiagnosticManager::addCheck(const QString &id, const QString &group, const QString &title, const QString &status,
                                 const QString &detail, const QString &remedy, const QString &action)
{
    m_checks << QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("group"), group}, {QStringLiteral("title"), title},
                            {QStringLiteral("status"), status}, {QStringLiteral("detail"), detail},
                            {QStringLiteral("remedy"), remedy}, {QStringLiteral("action"), action}};
}

void DiagnosticManager::refreshGnomeRemoteDesktop()
{
    // GNOME has its own RDP server (Settings → System → Remote Desktop) that also uses port 3389.
    ProcessRunner::run(this, QStringLiteral("grdctl"), {QStringLiteral("status")}, 10000, [this](const ProcessResult &r) {
        const QString out = QString::fromUtf8(r.stdOut);
        const qsizetype rdp = out.indexOf(QLatin1String("RDP:"));
        const bool enabled = rdp >= 0 && out.mid(rdp, 120).contains(QLatin1String("Status: enabled"));
        if (enabled != m_gnomeSharingEnabled) {
            m_gnomeSharingEnabled = enabled;
            m_debounce.start();
        }
    });
}

void DiagnosticManager::runChecks()
{
    m_checks.clear();
    const QString pass = QStringLiteral("pass"), warn = QStringLiteral("warn"), fail = QStringLiteral("fail"), info = QStringLiteral("info");

    // ---- software ----
    const QString inst = QStringLiteral("Software");
    if (m_config->xrdpInstalled())
        addCheck(QStringLiteral("xrdp"), inst, QStringLiteral("Remote desktop server is installed"), pass,
                 QStringLiteral("Version %1").arg(m_config->xrdpPackageVersion()));
    else
        addCheck(QStringLiteral("xrdp"), inst, QStringLiteral("Remote desktop server is not installed"), fail,
                 QStringLiteral("Without it nobody can connect to this computer."),
                 QStringLiteral("Open Setup and install it."), QStringLiteral("open-installation"));
    if (!m_config->xorgxrdpPackageVersion().isEmpty())
        addCheck(QStringLiteral("xorgxrdp"), inst, QStringLiteral("Display support is installed"), pass,
                 QStringLiteral("Version %1").arg(m_config->xorgxrdpPackageVersion()));
    else
        addCheck(QStringLiteral("xorgxrdp"), inst, QStringLiteral("Display support is missing"), fail,
                 QStringLiteral("People would not get a working screen when they connect."),
                 QStringLiteral("Open Setup and install \"Display support\"."), QStringLiteral("open-installation"));
    const QStringList desktops = m_config->installedDesktops();
    if (desktops.isEmpty())
        addCheck(QStringLiteral("desktop"), inst, QStringLiteral("No desktop is installed for remote users"), fail,
                 QStringLiteral("People would see a black screen after signing in."),
                 QStringLiteral("Open Setup and install Xfce."), QStringLiteral("open-installation"));
    else if (m_config->desktopId() == QLatin1String("xfce"))
        addCheck(QStringLiteral("desktop"), inst, QStringLiteral("Remote users get the Xfce desktop"), pass,
                 QStringLiteral("Xfce works well when many people are connected at the same time."));
    else
        addCheck(QStringLiteral("desktop"), inst, QStringLiteral("Remote users get the standard desktop"), info,
                 QStringLiteral("Installed desktops: %1.").arg(desktops.join(QStringLiteral(", "))),
                 QStringLiteral("For several people at once, choose Xfce in Settings → \"What people see when they connect\"."),
                 QStringLiteral("open-configuration"));
    addCheck(QStringLiteral("helper"), inst, m_helper->available() ? QStringLiteral("Administrator tools are installed")
                                                                   : QStringLiteral("Administrator tools are missing"),
             m_helper->available() ? pass : fail,
             m_helper->available() ? QStringLiteral("Changes can be made after you enter your password.")
                                   : QStringLiteral("You can look at the status, but you cannot change anything."),
             m_helper->available() ? QString() : QStringLiteral("Reinstall Ubuntu RDP Manager."));

    // ---- server ----
    const QString svc = QStringLiteral("Remote desktop server");
    const QString st = m_server->plainStatus();
    if (st == QLatin1String("on"))
        addCheck(QStringLiteral("service"), svc, QStringLiteral("Remote Desktop is ON"), pass,
                 QStringLiteral("Running for %1.").arg(m_server->uptimeText()));
    else if (st == QLatin1String("problem"))
        addCheck(QStringLiteral("service"), svc, QStringLiteral("Remote Desktop has stopped because of a problem"), fail,
                 QStringLiteral("It tried to run but failed (%1).").arg(m_server->unitResult()),
                 QStringLiteral("Try turning it on again. If that fails, look at the Activity Log."), QStringLiteral("open-server"));
    else if (st != QLatin1String("not-installed"))
        addCheck(QStringLiteral("service"), svc, QStringLiteral("Remote Desktop is OFF"), warn,
                 QStringLiteral("Nobody can connect right now."), QStringLiteral("Turn it on."), QStringLiteral("open-server"));
    if (m_server->installed())
        addCheck(QStringLiteral("autostart"), svc, m_server->autostart() ? QStringLiteral("Starts automatically with the computer")
                                                                         : QStringLiteral("Does not start automatically"),
                 m_server->autostart() ? pass : info,
                 m_server->autostart() ? QStringLiteral("After a restart of this computer, people can connect again without you doing anything.")
                                       : QStringLiteral("After this computer restarts, someone has to turn Remote Desktop on again."));
    if (m_server->running())
        addCheck(QStringLiteral("listener"), svc, m_server->listening() ? QStringLiteral("Ready for connections")
                                                                        : QStringLiteral("Not accepting connections"),
                 m_server->listening() ? pass : fail,
                 m_server->listening() ? QStringLiteral("Waiting for connections on port %1.").arg(m_config->primaryPort())
                                       : QStringLiteral("The server runs, but nothing answers on port %1.").arg(m_config->primaryPort()),
                 m_server->listening() ? QString() : QStringLiteral("Check the network port in Settings, then restart Remote Desktop."));
    if (m_server->portConflict())
        addCheck(QStringLiteral("portconflict"), svc, QStringLiteral("Another program is using the remote desktop port"), fail,
                 QStringLiteral("Port %1 is taken by a different program, so Remote Desktop cannot start.").arg(m_config->primaryPort()),
                 QStringLiteral("Switch off the other remote desktop program (for example GNOME's Remote Desktop) or choose another port in Settings."));
    if (m_gnomeSharingEnabled)
        addCheck(QStringLiteral("gnomerdp"), svc, QStringLiteral("GNOME's own Remote Desktop sharing is also switched on"), warn,
                 QStringLiteral("Ubuntu has a separate built-in screen-sharing feature. Both want to use port 3389, so one of them will not work."),
                 QStringLiteral("Open Ubuntu Settings → System → Remote Desktop and switch \"Desktop Sharing\" off, or give it a different port."));

    // ---- security ----
    const QString sec = QStringLiteral("Security");
    const QString layer = m_config->securityLayer().toLower();
    addCheck(QStringLiteral("securitylayer"), sec,
             layer == QLatin1String("tls") ? QStringLiteral("All connections are encrypted")
             : layer == QLatin1String("rdp") ? QStringLiteral("Connections use an old, insecure method")
                                             : QStringLiteral("Connections are encrypted when possible"),
             layer == QLatin1String("tls") ? pass : layer == QLatin1String("rdp") ? fail : warn,
             layer == QLatin1String("tls") ? QStringLiteral("Only encrypted connections are accepted.")
                                           : QStringLiteral("If encryption ever stops working, connections would quietly continue without it."),
             layer == QLatin1String("tls") ? QString() : QStringLiteral("Settings → Security → Connection security → \"Encrypted only\"."),
             layer == QLatin1String("tls") ? QString() : QStringLiteral("open-configuration"));

    // Can the server read its private key? (Stock Ubuntu 24.04 cannot, and silently drops encryption.)
    const QString keyPath = m_config->keyPath();
    const QString resolvedKey = QFileInfo(keyPath).canonicalFilePath().isEmpty() ? QFileInfo(keyPath).symLinkTarget() : QFileInfo(keyPath).canonicalFilePath();
    const QString effectiveKey = resolvedKey.isEmpty() ? keyPath : resolvedKey;
    struct stat sb{};
    bool xrdpGroupExists = false;
    unsigned xrdpGid = 0;
    groupMembers("xrdp", &xrdpGroupExists, &xrdpGid);
    const QStringList sslCertMembers = groupMembers("ssl-cert");
    bool keyOk = false;
    QString keyProblem;
    if (::stat(QFile::encodeName(effectiveKey).constData(), &sb) == 0) {
        const bool groupReadable = sb.st_mode & S_IRGRP;
        if (sb.st_mode & S_IROTH) {
            keyOk = true;
            keyProblem = QStringLiteral("The secret key file can be read by every user on this computer (%1).").arg(effectiveKey);
        } else if ((xrdpGroupExists && sb.st_gid == xrdpGid && groupReadable)) {
            keyOk = true;
        } else {
            struct group *g = ::getgrgid(sb.st_gid);
            keyOk = groupReadable && g && QString::fromUtf8(g->gr_name) == QLatin1String("ssl-cert") && sslCertMembers.contains(QStringLiteral("xrdp"));
        }
    } else if (effectiveKey.startsWith(QLatin1String("/etc/ssl/private/"))) {
        keyOk = sslCertMembers.contains(QStringLiteral("xrdp"));
    }
    const qint64 since = m_server->uptimeSeconds() >= 0 ? QDateTime::currentMSecsSinceEpoch() - m_server->uptimeSeconds() * 1000 : 0;
    const int tlsSinceStart = since > 0 ? m_logs->countSince(QStringLiteral("tls-problem"), since) : 0;
    if (!keyOk)
        addCheck(QStringLiteral("tlskey"), sec, QStringLiteral("Encryption is not working"), fail,
                 QStringLiteral("The remote desktop server cannot open its security key, so connections are NOT protected. "
                                "This is a known problem of a fresh Ubuntu installation."),
                 QStringLiteral("Settings → Security certificate → \"Create new certificate\", then restart Remote Desktop."),
                 QStringLiteral("open-tls"));
    else if (!keyProblem.isEmpty() || tlsSinceStart > 0)
        addCheck(QStringLiteral("tlskey"), sec, QStringLiteral("Encryption needs attention"), warn,
                 keyProblem.isEmpty() ? counted("Encryption problems recorded since Remote Desktop started: %n.", tlsSinceStart) : keyProblem,
                 QStringLiteral("Create a new certificate in Settings."), QStringLiteral("open-tls"));
    else
        addCheck(QStringLiteral("tlskey"), sec, QStringLiteral("Encryption works"), pass,
                 QStringLiteral("The server can use its security key, and only it can."));

    const QVariantMap cert = m_config->certificateInfo();
    if (cert.contains(QStringLiteral("error"))) {
        addCheck(QStringLiteral("certificate"), sec, QStringLiteral("Security certificate is missing"), fail,
                 QStringLiteral("Connecting computers cannot check that they reached the right computer."),
                 QStringLiteral("Create a new certificate in Settings."), QStringLiteral("open-tls"));
    } else {
        const int days = cert.value(QStringLiteral("daysLeft")).toInt();
        const bool selfSigned = cert.value(QStringLiteral("selfSigned")).toBool();
        if (days < 0)
            addCheck(QStringLiteral("certificate"), sec, QStringLiteral("Security certificate has expired"), fail,
                     QStringLiteral("Connecting computers will show a warning or refuse to connect."),
                     QStringLiteral("Create a new certificate in Settings."), QStringLiteral("open-tls"));
        else if (days < 30)
            addCheck(QStringLiteral("certificate"), sec, QStringLiteral("Security certificate expires soon"), warn,
                     counted("It expires in %n day(s).", days),
                     QStringLiteral("Create a new certificate in Settings."), QStringLiteral("open-tls"));
        else
            addCheck(QStringLiteral("certificate"), sec, QStringLiteral("Security certificate is valid"), selfSigned ? info : pass,
                     selfSigned ? counted("Valid for %n more day(s). It was made on this computer, so connecting computers will ask "
                                          "whether to trust it the first time — that is normal.", days)
                                : counted("Valid for %n more day(s).", days));
    }
    const QString protos = m_config->value(QStringLiteral("security.tlsProtocols"));
    const bool legacy = protos.contains(QLatin1String("SSLv3")) || protos.contains(QLatin1String("TLSv1,")) || protos.contains(QLatin1String("TLSv1.1"))
                        || protos.trimmed() == QLatin1String("TLSv1");
    if (legacy)
        addCheck(QStringLiteral("tlsprotocols"), sec, QStringLiteral("Old encryption versions are allowed"), warn,
                 QStringLiteral("Outdated encryption versions are switched on (%1).").arg(protos),
                 QStringLiteral("Settings → Allowed encryption versions → keep only 1.2 and 1.3."), QStringLiteral("open-configuration"));
    addCheck(QStringLiteral("rootlogin"), sec, m_config->allowRootLogin() ? QStringLiteral("The \"root\" account may connect")
                                                                          : QStringLiteral("The \"root\" account may not connect"),
             m_config->allowRootLogin() ? warn : pass,
             m_config->allowRootLogin() ? QStringLiteral("The all-powerful administrator account can sign in remotely. Ubuntu allows this by default.")
                                        : QStringLiteral("Good: the all-powerful administrator account cannot sign in remotely."),
             m_config->allowRootLogin() ? QStringLiteral("Settings → Security → switch off \"Allow the root administrator account to connect\".") : QString(),
             m_config->allowRootLogin() ? QStringLiteral("open-configuration") : QString());
    if (m_users->everyoneLockedOut())
        addCheck(QStringLiteral("access"), sec, QStringLiteral("Nobody is allowed to connect"), fail,
                 QStringLiteral("Only chosen people may connect, but nobody is chosen."),
                 QStringLiteral("Open \"Who Can Connect\" and allow at least one person."), QStringLiteral("open-users"));
    else
        addCheck(QStringLiteral("access"), sec, m_users->restrictionEnabled() ? QStringLiteral("Only chosen people may connect")
                                                                              : QStringLiteral("Everyone with an account may connect"),
                 info,
                 m_users->restrictionEnabled() ? counted("Allowed: %n person(s).", int(m_users->groupMembers().size()))
                                               : QStringLiteral("Anyone who has a user account and password on this computer can connect."),
                 m_users->restrictionEnabled() ? QString() : QStringLiteral("To limit this, open \"Who Can Connect\"."),
                 m_users->restrictionEnabled() ? QString() : QStringLiteral("open-users"));

    // ---- network ----
    const QString net = QStringLiteral("Network");
    QString ufw = QStringLiteral("unknown");
    QFile ufwConf(QStringLiteral("/etc/ufw/ufw.conf"));
    if (ufwConf.open(QIODevice::ReadOnly))
        ufw = QString::fromUtf8(ufwConf.readAll()).contains(QStringLiteral("ENABLED=yes")) ? QStringLiteral("on") : QStringLiteral("off");
    if (m_config->exposesNonLoopback())
        addCheck(QStringLiteral("exposure"), net, QStringLiteral("Other computers on the network can connect"), ufw == QLatin1String("on") ? info : warn,
                 ufw == QLatin1String("on") ? QStringLiteral("The firewall is on. Make sure it only allows your own network.")
                                            : QStringLiteral("The firewall is off, so any computer that can reach this one may try to connect."),
                 QStringLiteral("Use it only inside your office network or through a VPN. Never make port %1 reachable from the internet. "
                                "Ask your IT person to set a firewall rule.").arg(m_config->primaryPort()));
    else
        addCheck(QStringLiteral("exposure"), net, QStringLiteral("Only this computer itself can connect"), info,
                 QStringLiteral("Remote Desktop is limited to this computer. Other computers cannot reach it."),
                 QStringLiteral("To allow other computers, change the network port setting."), QStringLiteral("open-configuration"));

    // ---- users & capacity ----
    const QString cap = QStringLiteral("Users and capacity");
    const QString lk = m_config->limitKind();
    if (lk == QLatin1String("denies-all") || lk == QLatin1String("invalid"))
        addCheck(QStringLiteral("limit"), cap, QStringLiteral("Nobody can connect because of the connection limit"), fail,
                 m_config->limitDescription(), QStringLiteral("Open Connection Limits and choose \"No limit\"."), QStringLiteral("open-limits"));
    else if (lk != QLatin1String("unknown"))
        addCheck(QStringLiteral("limit"), cap, QStringLiteral("Connection limit"), lk == QLatin1String("unlimited") ? pass : info,
                 m_config->limitDescription());
    const QString lvl = m_monitor->advisorLevel();
    addCheck(QStringLiteral("capacity"), cap,
             lvl == QLatin1String("critical") ? QStringLiteral("This computer is overloaded")
             : lvl == QLatin1String("warning") ? QStringLiteral("This computer is getting busy")
                                               : QStringLiteral("This computer has room for more people"),
             lvl == QLatin1String("critical") ? fail : lvl == QLatin1String("warning") ? warn : pass,
             m_monitor->advisorMessages().isEmpty()
                 ? counted("About %n more person(s) could work here at the same time.", m_monitor->estimatedAdditionalSessions())
                 : m_monitor->advisorMessages().join(QStringLiteral(" ")),
             lvl == QLatin1String("ok") ? QString() : QStringLiteral("Close desktops that are not used any more, or add memory."));
    if (m_monitor->diskFreeGiB() >= 0 && m_monitor->diskFreeGiB() < 2)
        addCheck(QStringLiteral("disk"), cap, QStringLiteral("The disk is almost full"), warn,
                 QStringLiteral("Only %1 GB free. People may not be able to save their work.").arg(m_monitor->diskFreeGiB(), 0, 'f', 1));
    if (!m_sessions->alsoLocalUsers().isEmpty())
        addCheck(QStringLiteral("alsolocal"), cap, QStringLiteral("Same account used here and remotely at the same time"), warn,
                 QStringLiteral("%1 is signed in at this computer's own screen and remotely at the same time. Programs that allow only one "
                                "copy per account (Google Chrome, Firefox, some file managers) open on the screen where they are already "
                                "running — so they can appear here instead of on the remote computer.")
                     .arg(m_sessions->alsoLocalUsers().join(QStringLiteral(", "))),
                 QStringLiteral("Give remote users their own account (Ubuntu Settings → System → Users), or sign out at this computer before "
                                "connecting remotely."));
    if (m_sessions->staleCount() > 0)
        addCheck(QStringLiteral("stale"), cap, QStringLiteral("Some desktops were left open"), warn,
                 counted("Not used any more: %n desktop(s). They still take up memory.", m_sessions->staleCount()),
                 QStringLiteral("Open \"Who's Connected\" and close them."), QStringLiteral("open-sessions"));

    // ---- recent events ----
    const QString lg = QStringLiteral("Problems in the last 24 hours");
    if (m_logs->needsElevation())
        addCheck(QStringLiteral("journal"), lg, QStringLiteral("The activity log needs your password"), info,
                 QStringLiteral("Your account is not allowed to read the system's activity log directly."),
                 QStringLiteral("Open the Activity Log and press \"Show log (asks for password)\"."), QStringLiteral("open-logs"));
    addCheck(QStringLiteral("authfail"), lg, QStringLiteral("Wrong passwords"), m_logs->authFailures24h() > 10 ? warn : info,
             m_logs->authFailures24h() > 10
                 ? counted("%n sign-in attempt(s) used a wrong password. Many failures can mean someone is guessing passwords.", m_logs->authFailures24h())
                 : counted("%n sign-in attempt(s) used a wrong password.", m_logs->authFailures24h()));
    if (m_logs->accessDenied24h() + m_logs->limitDenied24h() > 0)
        addCheck(QStringLiteral("denied"), lg, QStringLiteral("Some people were turned away"), warn,
                 QStringLiteral("%1 not on the allowed list, %2 because the connection limit was reached.")
                     .arg(m_logs->accessDenied24h()).arg(m_logs->limitDenied24h()), {}, QStringLiteral("open-logs"));
    if (m_logs->sessionFailures24h() > 0)
        addCheck(QStringLiteral("sessionfail"), lg, QStringLiteral("Some desktops failed to open"), warn,
                 counted("%n time(s) a desktop could not be started after signing in.", m_logs->sessionFailures24h()),
                 {}, QStringLiteral("open-logs"));
    if (m_logs->tlsProblems24h() > 0)
        addCheck(QStringLiteral("tlsevents"), lg, QStringLiteral("Encryption problems were recorded"), warn,
                 counted("%n encryption problem(s) in the last 24 hours (older ones may already be fixed).", m_logs->tlsProblems24h()),
                 {}, QStringLiteral("open-logs"));
    emit checksChanged();
}

// ---------------- connection test ----------------
void DiagnosticManager::runConnectionTest(const QString &host, int port)
{
    if (m_probe->running())
        return;
    m_stages.clear();
    m_summary = QStringLiteral("Testing %1 …").arg(host);
    m_currentRecord = {{QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate)},
                       {QStringLiteral("target"), QStringLiteral("%1:%2").arg(host).arg(port)}};
    emit testChanged();
    // For this machine the expected certificate is known: pin it rather than requiring a CA.
    QByteArray pin;
    if (isLocalTarget(host.trimmed())) {
        const auto certs = QSslCertificate::fromPath(m_config->certificatePath(), QSsl::Pem);
        if (!certs.isEmpty())
            pin = certs.first().digest(QCryptographicHash::Sha256);
    }
    m_probe->setPinnedSha256(pin);
    m_probe->start(host.trimmed(), port);
    emit testChanged();
}

void DiagnosticManager::recordManualResult(bool authenticated, bool sessionCreated, bool desktopRendered, const QString &note)
{
    if (m_history.isEmpty())
        return;
    QVariantMap rec = m_history.first().toMap();
    rec.insert(QStringLiteral("manual"), QVariantMap{{QStringLiteral("authenticated"), authenticated},
                                                     {QStringLiteral("sessionCreated"), sessionCreated},
                                                     {QStringLiteral("desktopRendered"), desktopRendered},
                                                     {QStringLiteral("note"), note.left(500)},
                                                     {QStringLiteral("recorded"), QDateTime::currentDateTime().toString(Qt::ISODate)}});
    m_history[0] = rec;
    saveHistory();
    emit testChanged();
    emit notification(QStringLiteral("success"), QStringLiteral("Your test result was saved."));
}

void DiagnosticManager::saveHistory()
{
    QSettings().setValue(QStringLiteral("diagnostics/testHistory"), m_history);
}

void DiagnosticManager::detectClient()
{
    for (const char *c : {"xfreerdp3", "sdl-freerdp3", "xfreerdp", "remmina"}) {
        if (!QStandardPaths::findExecutable(QString::fromLatin1(c)).isEmpty()) {
            m_client = QString::fromLatin1(c);
            break;
        }
    }
    emit clientChanged();
}

QString DiagnosticManager::localCertificateFingerprint() const
{
    const auto certs = QSslCertificate::fromPath(m_config->certificatePath(), QSsl::Pem);
    if (certs.isEmpty())
        return {};
    return QString::fromLatin1(certs.first().digest(QCryptographicHash::Sha256).toHex(':')).toLower();
}

bool DiagnosticManager::isLocalTarget(const QString &host) const
{
    if (host == QLatin1String("localhost") || host.compare(QSysInfo::machineHostName(), Qt::CaseInsensitive) == 0)
        return true;
    const QHostAddress a(host);
    if (a.isNull())
        return false;
    if (a.isLoopback())
        return true;
    for (const QHostAddress &local : QNetworkInterface::allAddresses())
        if (local.isEqual(a, QHostAddress::TolerantConversion))
            return true;
    return false;
}

bool DiagnosticManager::launchClient(const QString &host, int port)
{
    detectClient();
    if (m_client.isEmpty()) {
        emit notification(QStringLiteral("warning"), QStringLiteral("No test program is installed. Install \"Test program\" on the Setup page."));
        return false;
    }
    QStringList args;
    if (m_client == QLatin1String("remmina")) {
        args << QStringLiteral("-c") << QStringLiteral("rdp://%1:%2").arg(host).arg(port);
    } else {
        args << QStringLiteral("/v:%1:%2").arg(host).arg(port) << QStringLiteral("/dynamic-resolution");
        // For this machine we know the certificate, so pin its fingerprint instead of trusting
        // blindly. Remote targets use FreeRDP's normal verification.
        if (m_client.endsWith(QLatin1Char('3')) && isLocalTarget(host)) {
            const QString fp = localCertificateFingerprint();
            if (!fp.isEmpty())
                args << QStringLiteral("/cert:fingerprint:sha256:%1").arg(fp);
        }
    }
    QString err;
    if (!ProcessRunner::startDetached(m_client, args, &err)) {
        emit notification(QStringLiteral("error"), QStringLiteral("Could not start %1: %2").arg(m_client, err));
        return false;
    }
    emit notification(QStringLiteral("info"), QStringLiteral("A remote desktop window is opening. Sign in with a user name and password, then tick what worked."));
    return true;
}

// ---------------- bundle ----------------
void DiagnosticManager::exportBundle(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    QString out = url.isLocalFile() ? url.toLocalFile() : fileUrl;
    if (!out.endsWith(QLatin1String(".tar.gz")))
        out += QStringLiteral(".tar.gz");
    auto tmp = std::make_shared<QTemporaryDir>();
    if (!tmp->isValid()) {
        emit notification(QStringLiteral("error"), QStringLiteral("Cannot create a temporary directory"));
        return;
    }
    m_exporting = true;
    emit exportingChanged();
    runChecks();

    auto write = [&](const QString &name, const QByteArray &data) {
        QFile f(tmp->filePath(name));
        if (f.open(QIODevice::WriteOnly))
            f.write(data);
    };
    QJsonObject report;
    report.insert(QStringLiteral("generated"), QDateTime::currentDateTime().toString(Qt::ISODate));
    report.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
    report.insert(QStringLiteral("kernel"), QSysInfo::kernelVersion());
    report.insert(QStringLiteral("arch"), QSysInfo::currentCpuArchitecture());
    report.insert(QStringLiteral("xrdp"), m_config->xrdpPackageVersion());
    report.insert(QStringLiteral("xorgxrdp"), m_config->xorgxrdpPackageVersion());
    report.insert(QStringLiteral("service"), QJsonObject{{QStringLiteral("status"), m_server->statusText()},
                                                         {QStringLiteral("autostart"), m_server->autostart()},
                                                         {QStringLiteral("listening"), QJsonArray::fromStringList(m_server->listeningEndpoints())}});
    report.insert(QStringLiteral("sessions"), QJsonObject{{QStringLiteral("total"), m_sessions->totalCount()},
                                                          {QStringLiteral("connected"), m_sessions->connectedCount()},
                                                          {QStringLiteral("disconnected"), m_sessions->disconnectedCount()}});
    report.insert(QStringLiteral("resources"), QJsonObject{{QStringLiteral("memTotalMiB"), m_monitor->memTotalMiB()},
                                                           {QStringLiteral("memAvailableMiB"), m_monitor->memAvailableMiB()},
                                                           {QStringLiteral("cpuCount"), m_monitor->cpuCount()},
                                                           {QStringLiteral("load1"), m_monitor->load1()},
                                                           {QStringLiteral("advisor"), m_monitor->advisorLevel()}});
    report.insert(QStringLiteral("checks"), QJsonArray::fromVariantList(m_checks));
    report.insert(QStringLiteral("connectionTests"), QJsonArray::fromVariantList(m_history));
    write(QStringLiteral("report.json"), QJsonDocument(report).toJson());
    for (const char *f : {"xrdp.ini", "sesman.ini"}) {
        QFile in(QStringLiteral("/etc/xrdp/") + QString::fromLatin1(f));
        if (in.open(QIODevice::ReadOnly))
            write(QString::fromLatin1(f), LogSanitizer::sanitize(QString::fromUtf8(in.readAll())).toUtf8());
    }
    write(QStringLiteral("journal.txt"), m_logs->sanitizedText(5000).toUtf8());
    write(QStringLiteral("README.txt"),
          "Ubuntu RDP Manager diagnostics bundle.\nSecrets (passwords, tokens, private keys) were redacted.\n"
          "Private key files are never included. User names and client IP addresses are included.\n");

    ProcessRunner::run(this, QStringLiteral("tar"), {QStringLiteral("-czf"), out, QStringLiteral("-C"), tmp->path(), QStringLiteral(".")}, 60000,
                       [this, tmp, out](const ProcessResult &r) {
                           m_exporting = false;
                           emit exportingChanged();
                           if (r.ok())
                               emit notification(QStringLiteral("success"), QStringLiteral("Support report saved to %1").arg(out));
                           else
                               emit notification(QStringLiteral("error"), QStringLiteral("tar failed: %1").arg(QString::fromUtf8(r.stdErr).trimmed()));
                       });
}

} // namespace urdm::app
