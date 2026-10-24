#pragma once

#include "DiagnosticManager.h"
#include "InstallationManager.h"
#include "LogManager.h"
#include "RdpConfigurationManager.h"
#include "RdpServerManager.h"
#include "SessionManager.h"
#include "SystemMonitor.h"
#include "UserAccessManager.h"

#include <QObject>
#include <QQmlEngine>

namespace urdm::app {

class HelperClient;
class SystemdClient;
class LogindClient;

// Composition root and the single QML entry point ("App"). Every dependency is created here
// and injected through constructors; QML only sees these service objects.
class ApplicationController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON
    Q_PROPERTY(RdpServerManager *server READ server CONSTANT)
    Q_PROPERTY(RdpConfigurationManager *config READ config CONSTANT)
    Q_PROPERTY(SessionManager *sessions READ sessions CONSTANT)
    Q_PROPERTY(UserAccessManager *users READ users CONSTANT)
    Q_PROPERTY(SystemMonitor *monitor READ monitor CONSTANT)
    Q_PROPERTY(LogManager *logs READ logs CONSTANT)
    Q_PROPERTY(DiagnosticManager *diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(InstallationManager *installer READ installer CONSTANT)
    Q_PROPERTY(bool helperAvailable READ helperAvailable NOTIFY helperAvailableChanged)
    Q_PROPERTY(bool helperBusy READ helperBusy NOTIFY helperBusyChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool themeRestartRequired READ themeRestartRequired NOTIFY themeModeChanged)
    Q_PROPERTY(bool sidebarCollapsed READ sidebarCollapsed WRITE setSidebarCollapsed NOTIFY sidebarCollapsedChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)

public:
    explicit ApplicationController(QString startupTheme, QObject *parent = nullptr);
    ~ApplicationController() override;

    // QML singleton factory: returns the instance created in main().
    static ApplicationController *create(QQmlEngine *, QJSEngine *);
    static void setInstance(ApplicationController *instance);

    RdpServerManager *server() const { return m_server; }
    RdpConfigurationManager *config() const { return m_config; }
    SessionManager *sessions() const { return m_sessions; }
    UserAccessManager *users() const { return m_users; }
    SystemMonitor *monitor() const { return m_monitor; }
    LogManager *logs() const { return m_logs; }
    DiagnosticManager *diagnostics() const { return m_diagnostics; }
    InstallationManager *installer() const { return m_installer; }
    bool helperAvailable() const;
    bool helperBusy() const;
    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &mode);
    bool themeRestartRequired() const { return m_themeMode != m_startupTheme; }
    Q_INVOKABLE void restartApplication();
    bool sidebarCollapsed() const { return m_sidebarCollapsed; }
    void setSidebarCollapsed(bool c);
    int currentPage() const { return m_currentPage; }
    void setCurrentPage(int p);
    QString version() const;
    QString qtVersion() const;

signals:
    void helperAvailableChanged();
    void helperBusyChanged();
    void themeModeChanged();
    void sidebarCollapsedChanged();
    void currentPageChanged();
    void notify(const QString &level, const QString &text);

private:
    void forward(QObject *source);

    HelperClient *m_helper;
    SystemdClient *m_systemd;
    LogindClient *m_logind;
    SystemMonitor *m_monitor;
    RdpConfigurationManager *m_config;
    RdpServerManager *m_server;
    SessionManager *m_sessions;
    LogManager *m_logs;
    UserAccessManager *m_users;
    DiagnosticManager *m_diagnostics;
    InstallationManager *m_installer;
    QString m_themeMode;
    QString m_startupTheme;
    bool m_sidebarCollapsed = false;
    int m_currentPage = 0;
};

} // namespace urdm::app
