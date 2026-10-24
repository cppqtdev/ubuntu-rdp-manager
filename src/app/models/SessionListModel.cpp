#include "SessionListModel.h"

namespace urdm::app {

namespace {
QString duration(const QDateTime &from)
{
    if (!from.isValid())
        return QStringLiteral("—");
    const qint64 s = from.secsTo(QDateTime::currentDateTime());
    if (s < 60)
        return QStringLiteral("%1s").arg(s);
    if (s < 3600)
        return QStringLiteral("%1m").arg(s / 60);
    if (s < 86400)
        return QStringLiteral("%1h %2m").arg(s / 3600).arg((s % 3600) / 60);
    return QStringLiteral("%1d %2h").arg(s / 86400).arg((s % 86400) / 3600);
}
} // namespace

int SessionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const SessionRow &r = m_rows.at(index.row());
    switch (role) {
    case SessionIdRole: return r.sessionId;
    case UserNameRole: return r.userName;
    case UidRole: return r.uid;
    case DisplayRole: return r.display;
    case StateRole: return r.state;
    case StartedRole: return r.started;
    case StartedTextRole: return r.started.isValid() ? r.started.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("—");
    case DurationTextRole: return duration(r.started);
    case DisconnectedSinceRole:
        return r.disconnectedSince.isValid() ? r.disconnectedSince.toString(QStringLiteral("HH:mm")) : QString();
    case ClientAddressRole: return r.clientAddress;
    case LeaderPidRole: return r.leaderPid;
    case MemoryRole: return r.memoryMiB;
    case CpuRole: return r.cpuPercent;
    case ProcessCountRole: return r.processCount;
    case StaleRole: return r.stale;
    case BackendRole: return QStringLiteral("Xorg (xorgxrdp)");
    default: return {};
    }
}

QHash<int, QByteArray> SessionListModel::roleNames() const
{
    return {{SessionIdRole, "sessionId"}, {UserNameRole, "userName"}, {UidRole, "uid"}, {DisplayRole, "display"},
            {StateRole, "sessionState"}, {StartedRole, "started"}, {StartedTextRole, "startedText"}, {DurationTextRole, "durationText"},
            {DisconnectedSinceRole, "disconnectedSince"}, {ClientAddressRole, "clientAddress"}, {LeaderPidRole, "leaderPid"},
            {MemoryRole, "memoryMiB"}, {CpuRole, "cpuPercent"}, {ProcessCountRole, "processCount"}, {StaleRole, "stale"},
            {BackendRole, "backend"}};
}

void SessionListModel::setRows(const QList<SessionRow> &rows)
{
    // Same identities in the same order: update in place so views keep selection/scroll.
    bool sameKeys = rows.size() == m_rows.size();
    for (qsizetype i = 0; sameKeys && i < rows.size(); ++i)
        sameKeys = rows.at(i).sessionId == m_rows.at(i).sessionId;
    if (sameKeys) {
        m_rows = rows;
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        return;
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}

} // namespace urdm::app
