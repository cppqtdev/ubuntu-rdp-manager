#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

namespace urdm::app {

class RdpConfigurationManager;
class RdpServerManager;
class SystemMonitor;
class LogManager;
class SessionManager;
class UserAccessManager;
class HelperClient;
class RdpProbe;

// Health checks computed from real system state, the staged connection test, and an
// exportable, sanitized diagnostics bundle.
class DiagnosticManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(QVariantList checks READ checks NOTIFY checksChanged)
    Q_PROPERTY(int failCount READ failCount NOTIFY checksChanged)
    Q_PROPERTY(int warnCount READ warnCount NOTIFY checksChanged)
    Q_PROPERTY(bool testRunning READ testRunning NOTIFY testChanged)
    Q_PROPERTY(QVariantList testStages READ testStages NOTIFY testChanged)
    Q_PROPERTY(QString testSummary READ testSummary NOTIFY testChanged)
    Q_PROPERTY(QVariantList testHistory READ testHistory NOTIFY testChanged)
    Q_PROPERTY(QString availableClient READ availableClient NOTIFY clientChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)

public:
    DiagnosticManager(RdpConfigurationManager *config, RdpServerManager *server, SystemMonitor *monitor, LogManager *logs,
                      SessionManager *sessions, UserAccessManager *users, HelperClient *helper, QObject *parent = nullptr);

    QVariantList checks() const { return m_checks; }
    int failCount() const;
    int warnCount() const;
    bool testRunning() const;
    QVariantList testStages() const { return m_stages; }
    QString testSummary() const { return m_summary; }
    QVariantList testHistory() const { return m_history; }
    QString availableClient() const { return m_client; }
    bool exporting() const { return m_exporting; }

    Q_INVOKABLE void runChecks();
    Q_INVOKABLE void runConnectionTest(const QString &host, int port);
    Q_INVOKABLE bool launchClient(const QString &host, int port);
    Q_INVOKABLE void recordManualResult(bool authenticated, bool sessionCreated, bool desktopRendered, const QString &note);
    Q_INVOKABLE void exportBundle(const QString &fileUrl);
    Q_INVOKABLE QString localCertificateFingerprint() const;

signals:
    void checksChanged();
    void testChanged();
    void clientChanged();
    void exportingChanged();
    void notification(const QString &level, const QString &text);

private:
    void addCheck(const QString &id, const QString &group, const QString &title, const QString &status,
                  const QString &detail, const QString &remedy = {}, const QString &action = {});
    void detectClient();
    void refreshGnomeRemoteDesktop();
    bool isLocalTarget(const QString &host) const;
    void saveHistory();

    RdpConfigurationManager *m_config;
    RdpServerManager *m_server;
    SystemMonitor *m_monitor;
    LogManager *m_logs;
    SessionManager *m_sessions;
    UserAccessManager *m_users;
    HelperClient *m_helper;
    RdpProbe *m_probe;
    QVariantList m_checks;
    QVariantList m_stages;
    QString m_summary;
    QVariantList m_history;
    QVariantMap m_currentRecord;
    QString m_client;
    bool m_exporting = false;
    bool m_gnomeSharingEnabled = false;
    QTimer m_debounce;
};

} // namespace urdm::app
