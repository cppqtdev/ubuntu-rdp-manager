#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>

namespace urdm::app {

class HelperClient;
class RdpConfigurationManager;
class SessionManager;
class LogManager;

struct UserRow
{
    QString name;
    uint uid = 0;
    QString fullName;
    QString home;
    QString shell;
    bool isAdmin = false;     // member of sudo/admin
    bool rdpAllowed = true;
    int sessionCount = 0;
    QString lastDenied;       // most recent "login denied" seen in the journal
};

class UserListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, UidRole, FullNameRole, HomeRole, ShellRole, AdminRole, AllowedRole, SessionsRole, DeniedRole };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(m_rows.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(m_rows.size()); }
    void setRows(const QList<UserRow> &rows);
    const QList<UserRow> &rows() const { return m_rows; }
signals:
    void countChanged();
private:
    QList<UserRow> m_rows;
};

// RDP access is controlled with existing Linux mechanisms only: PAM authentication plus the
// sesman TerminalServerUsers group. No passwords are read, stored or shown.
class UserAccessManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(UserListModel *model READ model CONSTANT)
    Q_PROPERTY(bool restrictionEnabled READ restrictionEnabled NOTIFY changed)
    Q_PROPERTY(QString accessGroup READ accessGroup NOTIFY changed)
    Q_PROPERTY(bool groupExists READ groupExists NOTIFY changed)
    Q_PROPERTY(bool everyoneLockedOut READ everyoneLockedOut NOTIFY changed)
    Q_PROPERTY(QStringList groupMembers READ groupMembers NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    UserAccessManager(HelperClient *helper, RdpConfigurationManager *config, SessionManager *sessions, LogManager *logs,
                      QObject *parent = nullptr);

    UserListModel *model() const { return m_model; }
    bool restrictionEnabled() const;
    QString accessGroup() const;
    bool groupExists() const { return m_groupExists; }
    bool everyoneLockedOut() const;
    QStringList groupMembers() const { return m_members; }
    bool busy() const { return m_busy; }
    bool loading() const { return m_watcher.isRunning(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void enableRestriction(const QString &groupName, const QStringList &initialUsers);
    Q_INVOKABLE void disableRestriction();
    Q_INVOKABLE void setUserAllowed(const QString &user, bool allowed);

signals:
    void changed();
    void busyChanged();
    void notification(const QString &level, const QString &text);

private:
    struct Snapshot {
        QList<UserRow> users;
        bool groupExists = false;
        QStringList members;
    };
    static Snapshot collect(const QString &group);
    void publish();
    void op(const QString &method, const QVariantList &args);

    HelperClient *m_helper;
    RdpConfigurationManager *m_config;
    SessionManager *m_sessions;
    LogManager *m_logs;
    UserListModel *m_model;
    QFutureWatcher<Snapshot> m_watcher;
    Snapshot m_snapshot;
    bool m_groupExists = false;
    QStringList m_members;
    bool m_busy = false;
};

} // namespace urdm::app
