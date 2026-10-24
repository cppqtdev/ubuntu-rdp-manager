#include "ApplicationController.h"

#include "DiagnosticManager.h"
#include "HelperClient.h"
#include "InstallationManager.h"
#include "LogManager.h"
#include "LogindClient.h"
#include "RdpConfigurationManager.h"
#include "RdpServerManager.h"
#include "SessionManager.h"
#include "SystemMonitor.h"
#include "SystemdClient.h"
#include "UserAccessManager.h"

#include <QDBusConnection>
#include <QDateTime>
#include <QGuiApplication>
#include <QPointer>
#include <QSettings>
#include <QProcess>

namespace urdm::app {

namespace {
QPointer<ApplicationController> g_instance;
}

ApplicationController::ApplicationController(QString startupTheme, QObject *parent)
    : QObject(parent)
    , m_startupTheme(std::move(startupTheme))
{
    QDBusConnection system = QDBusConnection::systemBus();
    m_helper = new HelperClient(system, this);
    m_systemd = new SystemdClient(system, this);
    m_logind = new LogindClient(system, this);
    m_monitor = new SystemMonitor(this);
    m_config = new RdpConfigurationManager(m_helper, RdpConfigurationManager::Paths{}, this);
    m_server = new RdpServerManager(m_systemd, m_helper, m_monitor, m_config, this);
    m_sessions = new SessionManager(m_logind, m_monitor, m_helper, this);
    m_logs = new LogManager(m_helper, this);
    m_users = new UserAccessManager(m_helper, m_config, m_sessions, m_logs, this);
    m_diagnostics = new DiagnosticManager(m_config, m_server, m_monitor, m_logs, m_sessions, m_users, m_helper, this);
    m_installer = new InstallationManager(m_helper, m_config, m_systemd, this);

    for (QObject *o : std::initializer_list<QObject *>{m_server, m_config, m_sessions, m_logs, m_users, m_diagnostics, m_installer})
        forward(o);
    connect(m_helper, &HelperClient::availableChanged, this, &ApplicationController::helperAvailableChanged);
    connect(m_helper, &HelperClient::pendingCallsChanged, this, &ApplicationController::helperBusyChanged);
    // A (re)start of xrdp after a configuration change clears "restart required".
    connect(m_server, &RdpServerManager::changed, this, [this]() {
        if (m_server->running() && m_server->uptimeSeconds() >= 0)
            m_config->noteServiceStarted((QDateTime::currentMSecsSinceEpoch() / 1000 - m_server->uptimeSeconds()) * 1000 + 999);
    });
    connect(m_systemd, &SystemdClient::unitChanged, this, [this](const QString &unit) {
        if (unit != QLatin1String("xrdp-sesman.service"))
            return;
        const UnitSnapshot s = m_systemd->unit(unit);
        if (s.valid)
            m_sessions->setSesmanState(s.props.activeState == QLatin1String("active"), s.activeEnterUsec);
    });
    // Configuration on disk changes after package installs; refresh dependents.
    connect(m_config, &RdpConfigurationManager::versionChanged, m_systemd, &SystemdClient::refreshAll);

    QSettings s;
    m_themeMode = m_startupTheme;
    m_sidebarCollapsed = s.value(QStringLiteral("ui/sidebarCollapsed"), false).toBool();
    m_currentPage = s.value(QStringLiteral("ui/page"), 0).toInt();
    m_sessions->setStaleHours(s.value(QStringLiteral("sessions/staleHours"), 24).toInt());
    connect(m_sessions, &SessionManager::staleHoursChanged, this,
            [this]() { QSettings().setValue(QStringLiteral("sessions/staleHours"), m_sessions->staleHours()); });
}

ApplicationController::~ApplicationController() = default;

ApplicationController *ApplicationController::create(QQmlEngine *, QJSEngine *engine)
{
    Q_ASSERT(g_instance);
    // The instance is owned by main(); the engine must never delete it.
    QJSEngine::setObjectOwnership(g_instance, QJSEngine::CppOwnership);
    Q_UNUSED(engine)
    return g_instance;
}

void ApplicationController::setInstance(ApplicationController *instance)
{
    g_instance = instance;
}

void ApplicationController::forward(QObject *source)
{
    // every service exposes notification(level, text)
    connect(source, SIGNAL(notification(QString,QString)), this, SIGNAL(notify(QString,QString)));
}

bool ApplicationController::helperAvailable() const
{
    return m_helper->available();
}

bool ApplicationController::helperBusy() const
{
    return m_helper->pendingCalls() > 0;
}

void ApplicationController::setThemeMode(const QString &mode)
{
    if (mode == m_themeMode || !(mode == QLatin1String("system") || mode == QLatin1String("light") || mode == QLatin1String("dark")))
        return;
    m_themeMode = mode;
    QSettings().setValue(QStringLiteral("ui/theme"), mode);
    emit themeModeChanged();
}

void ApplicationController::restartApplication()
{
    // The colour scheme is fixed at start-up on Linux (see main.cpp), so restart to apply it.
    QStringList args = QCoreApplication::arguments().mid(1);
    const qsizetype i = args.indexOf(QStringLiteral("--theme"));
    if (i >= 0)
        args.remove(i, std::min<qsizetype>(2, args.size() - i));
    if (QProcess::startDetached(QCoreApplication::applicationFilePath(), args))
        QCoreApplication::quit();
}

void ApplicationController::setSidebarCollapsed(bool c)
{
    if (c == m_sidebarCollapsed)
        return;
    m_sidebarCollapsed = c;
    QSettings().setValue(QStringLiteral("ui/sidebarCollapsed"), c);
    emit sidebarCollapsedChanged();
}

void ApplicationController::setCurrentPage(int p)
{
    if (p == m_currentPage)
        return;
    m_currentPage = p;
    QSettings().setValue(QStringLiteral("ui/page"), p);
    emit currentPageChanged();
}

QString ApplicationController::version() const
{
    return QCoreApplication::applicationVersion();
}

QString ApplicationController::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

} // namespace urdm::app
