#include "UserAccessManager.h"

#include "HelperClient.h"
#include "LogManager.h"
#include "RdpConfigurationManager.h"
#include "SessionManager.h"
#include "Validators.h"

#include <QFile>
#include <QRegularExpression>
#include <QtConcurrent>

#include <grp.h>
#include <pwd.h>

namespace urdm::app {

// ---------------- model ----------------
QVariant UserListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const UserRow &r = m_rows.at(index.row());
    switch (role) {
    case NameRole: return r.name;
    case UidRole: return r.uid;
    case FullNameRole: return r.fullName;
    case HomeRole: return r.home;
    case ShellRole: return r.shell;
    case AdminRole: return r.isAdmin;
    case AllowedRole: return r.rdpAllowed;
    case SessionsRole: return r.sessionCount;
    case DeniedRole: return r.lastDenied;
    default: return {};
    }
}

QHash<int, QByteArray> UserListModel::roleNames() const
{
    return {{NameRole, "name"}, {UidRole, "uid"}, {FullNameRole, "fullName"}, {HomeRole, "home"}, {ShellRole, "shell"},
            {AdminRole, "isAdmin"}, {AllowedRole, "rdpAllowed"}, {SessionsRole, "sessionCount"}, {DeniedRole, "lastDenied"}};
}

void UserListModel::setRows(const QList<UserRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}

// ---------------- manager ----------------
UserAccessManager::UserAccessManager(HelperClient *helper, RdpConfigurationManager *config, SessionManager *sessions,
                                     LogManager *logs, QObject *parent)
    : QObject(parent)
    , m_helper(helper)
    , m_config(config)
    , m_sessions(sessions)
    , m_logs(logs)
    , m_model(new UserListModel(this))
{
    connect(&m_watcher, &QFutureWatcher<Snapshot>::finished, this, [this]() {
        m_snapshot = m_watcher.result();
        m_groupExists = m_snapshot.groupExists;
        m_members = m_snapshot.members;
        publish();
    });
    connect(m_config, &RdpConfigurationManager::loaded, this, &UserAccessManager::refresh);
    connect(m_sessions, &SessionManager::countsChanged, this, &UserAccessManager::publish);
    connect(m_logs, &LogManager::countersChanged, this, &UserAccessManager::publish);
    refresh();
}

UserAccessManager::Snapshot UserAccessManager::collect(const QString &group)
{
    Snapshot s;
    int uidMin = 1000, uidMax = 60000;
    QFile defs(QStringLiteral("/etc/login.defs"));
    if (defs.open(QIODevice::ReadOnly)) {
        static const QRegularExpression re(QStringLiteral(R"(^\s*(UID_MIN|UID_MAX)\s+(\d+))"), QRegularExpression::MultilineOption);
        auto it = re.globalMatch(QString::fromUtf8(defs.readAll()));
        while (it.hasNext()) {
            const auto m = it.next();
            (m.captured(1) == QLatin1String("UID_MIN") ? uidMin : uidMax) = m.captured(2).toInt();
        }
    }
    auto groupMembers = [](const QString &name, bool *exists) {
        QStringList members;
        struct group grp{};
        struct group *res = nullptr;
        QByteArray buf(65536, '\0');
        if (::getgrnam_r(name.toUtf8().constData(), &grp, buf.data(), size_t(buf.size()), &res) == 0 && res) {
            if (exists)
                *exists = true;
            for (char **m = res->gr_mem; m && *m; ++m)
                members << QString::fromUtf8(*m);
        }
        return members;
    };
    QStringList admins = groupMembers(QStringLiteral("sudo"), nullptr) + groupMembers(QStringLiteral("admin"), nullptr);
    if (!group.isEmpty() && validators::isValidGroupName(group))
        s.members = groupMembers(group, &s.groupExists);

    // getpwent is not thread-safe, but this worker is its only user in the process.
    ::setpwent();
    while (struct passwd *pw = ::getpwent()) {
        if (int(pw->pw_uid) < uidMin || int(pw->pw_uid) > uidMax)
            continue;
        const QString shell = QString::fromUtf8(pw->pw_shell);
        if (shell.endsWith(QLatin1String("nologin")) || shell.endsWith(QLatin1String("/false")))
            continue;
        UserRow r;
        r.name = QString::fromUtf8(pw->pw_name);
        r.uid = pw->pw_uid;
        r.fullName = QString::fromUtf8(pw->pw_gecos).section(QLatin1Char(','), 0, 0);
        r.home = QString::fromUtf8(pw->pw_dir);
        r.shell = shell;
        r.isAdmin = admins.contains(r.name);
        s.users << r;
    }
    ::endpwent();
    std::sort(s.users.begin(), s.users.end(), [](const UserRow &a, const UserRow &b) { return a.name < b.name; });
    return s;
}

void UserAccessManager::refresh()
{
    if (m_watcher.isRunning())
        return;
    const QString group = m_config->accessGroup();
    m_watcher.setFuture(QtConcurrent::run([group]() { return collect(group); }));
    emit changed();
}

void UserAccessManager::publish()
{
    QList<UserRow> rows = m_snapshot.users;
    const bool restricted = restrictionEnabled();
    const QVariantMap denied = m_logs->lastDeniedByUser();
    for (UserRow &r : rows) {
        r.rdpAllowed = !restricted || m_members.contains(r.name);
        r.sessionCount = m_sessions->sessionCountForUser(r.name);
        r.lastDenied = denied.value(r.name).toString();
    }
    m_model->setRows(rows);
    emit changed();
}

QString UserAccessManager::accessGroup() const
{
    return m_config->accessGroup();
}

bool UserAccessManager::restrictionEnabled() const
{
    // sesman enforces TerminalServerUsers when the group exists, or always with AlwaysGroupCheck.
    return !accessGroup().isEmpty() && (m_groupExists || m_config->alwaysGroupCheck());
}

bool UserAccessManager::everyoneLockedOut() const
{
    return restrictionEnabled() && (!m_groupExists || m_members.isEmpty());
}

void UserAccessManager::op(const QString &method, const QVariantList &args)
{
    m_busy = true;
    emit busyChanged();
    m_helper->call(method, args, [this](const HelperReply &r) {
        m_busy = false;
        emit busyChanged();
        emit notification(r.ok ? QStringLiteral("success") : (r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error")),
                          r.message);
        m_config->reload();
        refresh();
    });
}

void UserAccessManager::enableRestriction(const QString &groupName, const QStringList &initialUsers)
{
    op(QStringLiteral("SetAccessRestriction"), {true, groupName.trimmed(), initialUsers});
}

void UserAccessManager::disableRestriction()
{
    op(QStringLiteral("SetAccessRestriction"), {false, QString(), QStringList()});
}

void UserAccessManager::setUserAllowed(const QString &user, bool allowed)
{
    op(QStringLiteral("SetUserAccess"), {user, allowed});
}

} // namespace urdm::app
