#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>
#include <optional>

namespace urdm::helper {

struct CommandResult
{
    int exitCode = -1;
    QByteArray stdOut;
    QByteArray stdErr;
    bool startFailed = false;
    bool timedOut = false;
    bool ok() const { return !startFailed && !timedOut && exitCode == 0; }
    QString errorText() const;
};

struct GroupInfo
{
    bool exists = false;
    unsigned gid = 0;
    QStringList members;
};

struct UserInfo
{
    bool exists = false;
    unsigned uid = 0;
    unsigned gid = 0;
};

struct FileOwnership
{
    bool exists = false;
    bool regularFile = false;
    bool symlink = false;
    unsigned uid = 0;
    unsigned gid = 0;
    unsigned mode = 0;
    qint64 size = 0;
};

// Every interaction of the helper with the operating system goes through this interface so
// that HelperCore can be unit-tested with a fake (tests/unit/tst_helpercore.cpp).
class SystemOps
{
public:
    virtual ~SystemOps() = default;

    // Runs a program directly (no shell) with a minimal, fixed environment plus `extraEnv`.
    virtual CommandResult run(const QString &program, const QStringList &args, const QHash<QString, QString> &extraEnv = {},
                              int timeoutMs = 60000) = 0;

    virtual QString packageVersion(const QString &package) = 0; // empty when not installed
    virtual bool fileExists(const QString &path) = 0;
    virtual FileOwnership stat(const QString &path) = 0;
    virtual std::optional<QByteArray> readFile(const QString &path, qint64 maxBytes) = 0;
    // Atomically writes a file and applies ownership/mode.
    virtual bool writeFile(const QString &path, const QByteArray &data, unsigned uid, unsigned gid, unsigned mode,
                           QString *error) = 0;

    virtual GroupInfo group(const QString &name) = 0;
    virtual UserInfo user(const QString &name) = 0;
    virtual QList<unsigned> supplementaryGroups(const QString &userName) = 0;

    // systemd (org.freedesktop.systemd1). Completion is reported asynchronously.
    using Completion = std::function<void(bool ok, QString message)>;
    virtual QString unitLoadState(const QString &unit) = 0;
    virtual void unitJob(const QString &method, const QString &unit, Completion done) = 0; // StartUnit/StopUnit/RestartUnit
    virtual bool setUnitFilesEnabled(const QStringList &units, bool enabled, QString *error) = 0;

    // logind (org.freedesktop.login1)
    virtual std::optional<QVariantMap> sessionProperties(const QString &sessionId) = 0;
    virtual bool terminateSession(const QString &sessionId, QString *error) = 0;

    // Maps X11 display number -> "address:port" of the attached RDP client, derived from kernel
    // socket tables (xrdp_display_N peer socket -> xrdp child process -> its TCP peer).
    virtual std::optional<QVariantMap> xrdpClientAddresses(QString *error) = 0;
};

} // namespace urdm::helper
