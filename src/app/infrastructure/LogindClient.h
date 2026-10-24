#pragma once

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QTimer>

namespace urdm::app {

struct LogindSession
{
    QString id;
    uint uid = 0;
    QString user;
    QString service;   // PAM service; "xrdp-sesman" for RDP sessions
    QString display;   // ":10"
    QString type;      // x11, tty, ...
    QString state;     // online, active, closing
    QString scope;     // session-c1.scope
    quint64 timestampUsec = 0;
    uint leader = 0;
    bool remote = false;
    QString remoteHost;
    QString objectPath;
};

// Unprivileged session enumeration via org.freedesktop.login1. Refreshes on SessionNew /
// SessionRemoved (debounced) and on demand.
class LogindClient : public QObject
{
    Q_OBJECT
public:
    explicit LogindClient(QDBusConnection bus, QObject *parent = nullptr);
    void refresh();
    QList<LogindSession> sessions() const { return m_sessions; }
    bool lastRefreshFailed() const { return m_failed; }

signals:
    void sessionsChanged();

private slots:
    void scheduleRefresh();

private:
    QDBusConnection m_bus;
    QList<LogindSession> m_sessions;
    QTimer m_debounce;
    bool m_failed = false;
    bool m_inFlight = false;
    bool m_again = false;
};

} // namespace urdm::app
