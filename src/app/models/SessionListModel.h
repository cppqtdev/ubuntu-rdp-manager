#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QQmlEngine>

namespace urdm::app {

struct SessionRow
{
    QString sessionId;
    QString userName;
    uint uid = 0;
    QString display;
    int displayNumber = -1;
    QString state;          // "Connected", "Disconnected", "Closing"
    QDateTime started;
    QDateTime disconnectedSince; // only when observed by this application
    QString clientAddress;  // empty until fetched through the helper
    uint leaderPid = 0;
    QString scope;
    double memoryMiB = -1;  // -1 = unavailable
    double cpuPercent = -1;
    int processCount = -1;
    bool stale = false;
};

class SessionListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        SessionIdRole = Qt::UserRole + 1, UserNameRole, UidRole, DisplayRole, StateRole, StartedRole, StartedTextRole,
        DurationTextRole, DisconnectedSinceRole, ClientAddressRole, LeaderPidRole, MemoryRole, CpuRole, ProcessCountRole,
        StaleRole, BackendRole
    };
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(m_rows.size()); }

    void setRows(const QList<SessionRow> &rows);
    const QList<SessionRow> &rows() const { return m_rows; }

signals:
    void countChanged();

private:
    QList<SessionRow> m_rows;
};

} // namespace urdm::app
