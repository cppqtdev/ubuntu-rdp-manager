#pragma once

#include "SessionListModel.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QSet>

namespace urdm::app {

class HelperClient;
class LogindClient;
class SystemMonitor;

// Remote sessions from verified sources only:
//   * session list:      logind sessions whose PAM service is xrdp-sesman
//   * connected/disconn: kernel unix-socket state of /run/xrdp/sockdir/xrdp_display_N
//   * resources:         the session's cgroup v2 scope (memory.current, cpu.stat)
//   * client address:    helper (root) via kernel socket tables, on request
class SessionManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(SessionListModel *model READ model CONSTANT)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY countsChanged)
    Q_PROPERTY(int disconnectedCount READ disconnectedCount NOTIFY countsChanged)
    Q_PROPERTY(int staleCount READ staleCount NOTIFY countsChanged)
    Q_PROPERTY(int orphanedCount READ orphanedCount NOTIFY countsChanged)
    Q_PROPERTY(QStringList loggedInUsers READ loggedInUsers NOTIFY countsChanged)
    Q_PROPERTY(bool available READ available NOTIFY countsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int staleHours READ staleHours WRITE setStaleHours NOTIFY staleHoursChanged)
    Q_PROPERTY(bool addressesLoaded READ addressesLoaded NOTIFY countsChanged)
    // users who are connected remotely AND signed in at this computer's own screen
    Q_PROPERTY(QStringList alsoLocalUsers READ alsoLocalUsers NOTIFY countsChanged)

public:
    SessionManager(LogindClient *logind, SystemMonitor *monitor, HelperClient *helper, QObject *parent = nullptr);

    SessionListModel *model() const { return m_model; }
    int totalCount() const { return int(m_model->rows().size()); }
    int connectedCount() const;
    int disconnectedCount() const { return totalCount() - connectedCount(); }
    int staleCount() const;
    int orphanedCount() const;
    // xrdp-sesman keeps its session list in memory only: sessions opened before the running
    // sesman started (or while it is stopped) can no longer be reconnected (verified on 0.9.24).
    void setSesmanState(bool running, quint64 activeEnterUsec);
    QStringList loggedInUsers() const;
    bool available() const { return m_available; }
    bool busy() const { return m_busy; }
    int staleHours() const { return m_staleHours; }
    void setStaleHours(int h);
    bool addressesLoaded() const { return m_addressesLoaded; }
    int sessionCountForUser(const QString &user) const;
    QStringList alsoLocalUsers() const { return m_alsoLocal; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void disconnectSession(const QString &sessionId);
    Q_INVOKABLE void terminateSession(const QString &sessionId);
    Q_INVOKABLE void terminateStaleSessions();
    Q_INVOKABLE void loadClientAddresses();
    Q_INVOKABLE QVariantMap sessionDetails(const QString &sessionId) const;

signals:
    void countsChanged();
    void busyChanged();
    void staleHoursChanged();
    void notification(const QString &level, const QString &text);

private:
    void rebuild();
    void sessionOp(const QString &method, const QString &sessionId, const QString &verb);

    LogindClient *m_logind;
    SystemMonitor *m_monitor;
    HelperClient *m_helper;
    SessionListModel *m_model;
    QHash<int, QString> m_addresses;          // display -> client
    QHash<QString, QDateTime> m_disconnectedSince;
    QSet<QString> m_seenConnected;
    bool m_available = true;
    bool m_busy = false;
    bool m_addressesLoaded = false;
    int m_staleHours = 24;
    QStringList m_alsoLocal;
    bool m_sesmanRunning = true;
    quint64 m_sesmanStartUsec = 0;
};

} // namespace urdm::app
