#include "InstallationManager.h"

#include "HelperClient.h"
#include "HelperProtocol.h"
#include "ProcessRunner.h"
#include "RdpConfigurationManager.h"
#include "SystemdClient.h"

#include <QFile>
#include <QSysInfo>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace urdm::app {

InstallationManager::InstallationManager(HelperClient *helper, RdpConfigurationManager *config, SystemdClient *systemd, QObject *parent)
    : QObject(parent)
    , m_helper(helper)
    , m_config(config)
    , m_systemd(systemd)
{
    readOsRelease();
    connect(m_helper, &HelperClient::availableChanged, this, &InstallationManager::stateChanged);
    connect(m_config, &RdpConfigurationManager::versionChanged, this, &InstallationManager::detectComponents);
    connect(m_helper, &HelperClient::jobOutput, this, [this](const QString &id, const QString &line) {
        if (id != m_jobId)
            return;
        m_log << line;
        while (m_log.size() > 2000)
            m_log.removeFirst();
        emit logChanged();
    });
    connect(m_helper, &HelperClient::jobFinished, this, [this](const QString &id, bool ok, int code, const QString &summary) {
        if (id != m_jobId)
            return;
        m_jobId.clear();
        m_succeeded = ok;
        m_result = ok ? QStringLiteral("Installation finished successfully.")
                      : QStringLiteral("Installation did not finish (%1, code %2). Check your internet connection and try again.").arg(summary).arg(code);
        emit notification(ok ? QStringLiteral("success") : QStringLiteral("error"), m_result);
        refresh();
        emit stateChanged();
    });
    detectComponents();
}

void InstallationManager::readOsRelease()
{
    QFile f(QStringLiteral("/etc/os-release"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (const QByteArray &line : f.readAll().split('\n')) {
        const qsizetype eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        const QByteArray key = line.left(eq);
        QString v = QString::fromUtf8(line.mid(eq + 1)).trimmed();
        if (v.startsWith(QLatin1Char('"')) && v.endsWith(QLatin1Char('"')) && v.size() >= 2)
            v = v.mid(1, v.size() - 2);
        if (key == "PRETTY_NAME")
            m_osName = v;
        else if (key == "ID")
            m_osId = v;
        else if (key == "VERSION_ID")
            m_osVersion = v;
    }
}

QString InstallationManager::osSupport() const
{
    if (m_osId != QLatin1String("ubuntu"))
        return QStringLiteral("unsupported");
    if (m_osVersion == QLatin1String("24.04"))
        return QStringLiteral("verified");
    return QStringLiteral("untested");
}

QString InstallationManager::architecture() const
{
    return QSysInfo::currentCpuArchitecture();
}

bool InstallationManager::architectureSupported() const
{
    const QString a = architecture();
    return a == QLatin1String("x86_64") || a == QLatin1String("arm64");
}

QString InstallationManager::userName() const
{
    if (struct passwd *pw = ::getpwuid(::getuid()))
        return QString::fromUtf8(pw->pw_name);
    return QString::number(::getuid());
}

bool InstallationManager::inGroup(const char *group) const
{
    struct group *g = ::getgrnam(group);
    if (!g)
        return false;
    gid_t groups[256];
    const int n = ::getgroups(256, groups);
    for (int i = 0; i < n; ++i)
        if (groups[i] == g->gr_gid)
            return true;
    return ::getgid() == g->gr_gid;
}

bool InstallationManager::userIsAdmin() const
{
    return inGroup("sudo") || inGroup("admin");
}

bool InstallationManager::userCanReadJournal() const
{
    return inGroup("adm") || inGroup("systemd-journal");
}

bool InstallationManager::helperAvailable() const
{
    return m_helper->available();
}

void InstallationManager::detectComponents()
{
    struct Comp { const char *id; const char *title; const char *package; const char *why; };
    static const Comp comps[] = {
        {"xrdp", "Remote desktop server", "xrdp", "The main program that lets other computers connect"},
        {"xorgxrdp", "Display support", "xorgxrdp", "Gives every person their own separate screen"},
        {"xfce", "Xfce desktop", "xfce4", "A light, fast desktop that works well for many people at once"},
        {"audio", "Sound and microphone", "pipewire-module-xrdp", "Plays sound on the person's speakers and uses their microphone"},
        {"test-client", "Test program (optional)", "freerdp3-x11", "Lets you try a remote connection from this computer"},
    };
    auto list = std::make_shared<QVariantList>();
    auto remaining = std::make_shared<int>(int(std::size(comps)));
    for (const Comp &c : comps) {
        const QString pkg = QString::fromLatin1(c.package);
        ProcessRunner::run(this, QStringLiteral("dpkg-query"),
                           {QStringLiteral("-W"), QStringLiteral("-f=${db:Status-Abbrev}|${Version}"), QStringLiteral("--"), pkg}, 15000,
                           [this, c, list, remaining](const ProcessResult &r) {
                               const QString out = QString::fromUtf8(r.stdOut).trimmed();
                               const bool installed = r.ok() && out.startsWith(QLatin1String("ii"));
                               list->append(QVariantMap{{QStringLiteral("id"), QString::fromLatin1(c.id)},
                                                        {QStringLiteral("title"), QString::fromLatin1(c.title)},
                                                        {QStringLiteral("package"), QString::fromLatin1(c.package)},
                                                        {QStringLiteral("why"), QString::fromLatin1(c.why)},
                                                        {QStringLiteral("installed"), installed},
                                                        {QStringLiteral("version"), installed ? out.section(QLatin1Char('|'), 1) : QString()},
                                                        {QStringLiteral("optional"), QString::fromLatin1(c.id) == QLatin1String("test-client")}});
                               if (--*remaining == 0) {
                                   static const QStringList order{QStringLiteral("xrdp"), QStringLiteral("xorgxrdp"), QStringLiteral("xfce"), QStringLiteral("audio"), QStringLiteral("test-client")};
                                   std::sort(list->begin(), list->end(), [](const QVariant &a, const QVariant &b) {
                                       return order.indexOf(a.toMap().value(QStringLiteral("id")).toString())
                                              < order.indexOf(b.toMap().value(QStringLiteral("id")).toString());
                                   });
                                   m_components = *list;
                                   emit stateChanged();
                               }
                           });
    }
}

void InstallationManager::refresh()
{
    m_helper->refreshAvailability();
    m_config->refreshVersions();
    m_config->reload();
    m_systemd->refreshAll();
    detectComponents();
}

void InstallationManager::install(const QStringList &components)
{
    if (installing() || components.isEmpty())
        return;
    m_log.clear();
    m_result.clear();
    m_succeeded = false;
    m_log << QStringLiteral("Asking for administrator permission to install: %1").arg(components.join(QStringLiteral(", ")));
    emit logChanged();
    m_helper->call(QStringLiteral("StartPackageInstall"), {components}, [this](const HelperReply &r) {
        if (!r.ok) {
            m_result = r.message;
            m_log << QStringLiteral("Not started: %1").arg(r.message);
            emit logChanged();
            emit notification(r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error"), r.message);
            emit stateChanged();
            return;
        }
        m_jobId = r.value.toString();
        m_log << QStringLiteral("Downloading and installing… this can take several minutes.");
        emit logChanged();
        emit stateChanged();
    });
}

} // namespace urdm::app
