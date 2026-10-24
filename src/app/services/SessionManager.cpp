#include "SessionManager.h"

#include "HelperClient.h"
#include "HelperProtocol.h"
#include "LogindClient.h"
#include "SystemMonitor.h"
#include "Validators.h"

#include <algorithm>

namespace urdm::app {

SessionManager::SessionManager(LogindClient *logind, SystemMonitor *monitor, HelperClient *helper, QObject *parent)
    : QObject(parent)
    , m_logind(logind)
    , m_monitor(monitor)
    , m_helper(helper)
    , m_model(new SessionListModel(this))
{
    connect(m_logind, &LogindClient::sessionsChanged, this, &SessionManager::rebuild);
    connect(m_monitor, &SystemMonitor::sampled, this, &SessionManager::rebuild);
    m_logind->refresh();
}

void SessionManager::refresh()
{
    m_logind->refresh();
    m_monitor->sampleNow();
}

void SessionManager::setStaleHours(int h)
{
    h = qBound(1, h, 24 * 30);
    if (h == m_staleHours)
        return;
    m_staleHours = h;
    emit staleHoursChanged();
    rebuild();
}

void SessionManager::rebuild()
{
    m_available = !m_logind->lastRefreshFailed();
    const SystemSample &sample = m_monitor->lastSample();
    const QDateTime now = QDateTime::currentDateTime();
    QList<SessionRow> rows;
    QList<ScopeRef> scopes;
    for (const LogindSession &s : m_logind->sessions()) {
        if (s.service != urdm::protocol::kSesmanPamService)
            continue;
        SessionRow r;
        r.sessionId = s.id;
        r.userName = s.user;
        r.uid = s.uid;
        r.display = s.display;
        r.displayNumber = validators::parseDisplayNumber(s.display).value_or(-1);
        r.started = s.timestampUsec ? QDateTime::fromMSecsSinceEpoch(qint64(s.timestampUsec / 1000)) : QDateTime();
        r.leaderPid = s.leader;
        r.scope = s.scope;
        const bool attached = r.displayNumber >= 0 && sample.attachedDisplays.contains(r.displayNumber);
        if (s.state == QLatin1String("closing")) {
            r.state = QStringLiteral("Closing");
        } else if (attached) {
            r.state = QStringLiteral("Connected");
            m_seenConnected.insert(s.id);
            m_disconnectedSince.remove(s.id);
        } else if (!m_sesmanRunning || (m_sesmanStartUsec > 0 && s.timestampUsec + 1000000 < m_sesmanStartUsec)) {
            r.state = QStringLiteral("Orphaned");
        } else {
            r.state = QStringLiteral("Disconnected");
            if (!m_disconnectedSince.contains(s.id) && m_seenConnected.contains(s.id))
                m_disconnectedSince.insert(s.id, now);
        }
        r.disconnectedSince = m_disconnectedSince.value(s.id);
        // stale: orphaned (unreachable), or disconnected for longer than staleHours
        const QDateTime ref = r.disconnectedSince.isValid() ? r.disconnectedSince : r.started;
        r.stale = r.state == QLatin1String("Orphaned")
                  || (r.state == QLatin1String("Disconnected") && ref.isValid() && ref.secsTo(now) > qint64(m_staleHours) * 3600);
        r.clientAddress = attached ? m_addresses.value(r.displayNumber) : QString();
        const ScopeUsage u = sample.scopes.value(s.id);
        r.memoryMiB = u.memoryBytes ? double(*u.memoryBytes) / 1048576.0 : -1;
        r.cpuPercent = m_monitor->sessionCpuPercent(s.id);
        r.processCount = u.processCount;
        rows << r;
        scopes << ScopeRef{s.id, s.uid, s.scope};
    }
    std::sort(rows.begin(), rows.end(), [](const SessionRow &a, const SessionRow &b) { return a.displayNumber < b.displayNumber; });
    QStringList alsoLocal;
    for (const LogindSession &s : m_logind->sessions()) {
        const bool localGraphical = s.service != urdm::protocol::kSesmanPamService
                                    && (s.type == QLatin1String("wayland") || s.type == QLatin1String("x11"));
        if (!localGraphical || alsoLocal.contains(s.user))
            continue;
        if (std::any_of(rows.cbegin(), rows.cend(), [&](const SessionRow &r) { return r.userName == s.user; }))
            alsoLocal << s.user;
    }
    m_alsoLocal = alsoLocal;
    m_monitor->setScopes(scopes);
    m_monitor->setSessionCount(int(rows.size()));
    m_model->setRows(rows);
    emit countsChanged();
}

int SessionManager::connectedCount() const
{
    int n = 0;
    for (const auto &r : m_model->rows())
        if (r.state == QLatin1String("Connected"))
            ++n;
    return n;
}

void SessionManager::setSesmanState(bool running, quint64 activeEnterUsec)
{
    if (running == m_sesmanRunning && activeEnterUsec == m_sesmanStartUsec)
        return;
    m_sesmanRunning = running;
    m_sesmanStartUsec = activeEnterUsec;
    rebuild();
}

int SessionManager::orphanedCount() const
{
    int n = 0;
    for (const auto &r : m_model->rows())
        n += r.state == QLatin1String("Orphaned");
    return n;
}

int SessionManager::staleCount() const
{
    int n = 0;
    for (const auto &r : m_model->rows())
        if (r.stale)
            ++n;
    return n;
}

QStringList SessionManager::loggedInUsers() const
{
    QStringList users;
    for (const auto &r : m_model->rows())
        if (!users.contains(r.userName))
            users << r.userName;
    users.sort();
    return users;
}

int SessionManager::sessionCountForUser(const QString &user) const
{
    int n = 0;
    for (const auto &r : m_model->rows())
        if (r.userName == user)
            ++n;
    return n;
}

QVariantMap SessionManager::sessionDetails(const QString &sessionId) const
{
    for (const auto &r : m_model->rows()) {
        if (r.sessionId != sessionId)
            continue;
        return {{QStringLiteral("sessionId"), r.sessionId}, {QStringLiteral("userName"), r.userName}, {QStringLiteral("uid"), r.uid},
                {QStringLiteral("display"), r.display}, {QStringLiteral("state"), r.state},
                {QStringLiteral("started"), r.started.toString(Qt::ISODate)}, {QStringLiteral("leaderPid"), r.leaderPid},
                {QStringLiteral("scope"), r.scope}, {QStringLiteral("clientAddress"), r.clientAddress},
                {QStringLiteral("memoryMiB"), r.memoryMiB}, {QStringLiteral("cpuPercent"), r.cpuPercent},
                {QStringLiteral("processCount"), r.processCount}, {QStringLiteral("stale"), r.stale},
                {QStringLiteral("disconnectedSince"), r.disconnectedSince.toString(Qt::ISODate)}};
    }
    return {};
}

void SessionManager::sessionOp(const QString &method, const QString &sessionId, const QString &verb)
{
    if (!validators::isValidLogindSessionId(sessionId))
        return;
    QString who = sessionId;
    for (const auto &row : m_model->rows())
        if (row.sessionId == sessionId)
            who = row.userName;
    m_busy = true;
    emit busyChanged();
    m_helper->call(method, {sessionId}, [this, verb, who](const HelperReply &r) {
        m_busy = false;
        emit busyChanged();
        const QString okText = verb == QLatin1String("disconnect")
            ? QStringLiteral("%1 was disconnected. Their programs stay open and they can connect again.").arg(who)
            : QStringLiteral("%1's remote desktop was closed.").arg(who);
        emit notification(r.ok ? QStringLiteral("success") : (r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error")),
                          r.ok ? okText : r.notAuthorized ? r.message
                                        : QStringLiteral("Could not %1 %2. Details: %3").arg(verb, who, r.message));
        refresh();
    });
}

void SessionManager::disconnectSession(const QString &sessionId)
{
    sessionOp(QStringLiteral("DisconnectSession"), sessionId, QStringLiteral("disconnect"));
}

void SessionManager::terminateSession(const QString &sessionId)
{
    sessionOp(QStringLiteral("TerminateSession"), sessionId, QStringLiteral("close the desktop of"));
}

void SessionManager::terminateStaleSessions()
{
    for (const auto &r : m_model->rows())
        if (r.stale)
            terminateSession(r.sessionId);
}

void SessionManager::loadClientAddresses()
{
    m_helper->call(QStringLiteral("GetClientAddresses"), {}, [this](const HelperReply &r) {
        if (!r.ok) {
            emit notification(r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error"),
                              r.notAuthorized ? r.message : QStringLiteral("Could not look up where people connect from. Details: %1").arg(r.message));
            return;
        }
        m_addresses.clear();
        const QVariantMap map = r.value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            m_addresses.insert(it.key().toInt(), it.value().toString());
        m_addressesLoaded = true;
        rebuild();
    });
}

} // namespace urdm::app
