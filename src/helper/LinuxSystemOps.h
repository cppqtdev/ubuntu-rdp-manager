#pragma once

#include "SystemOps.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QTimer>

namespace urdm::helper {

class LinuxSystemOps : public QObject, public SystemOps
{
    Q_OBJECT

public:
    explicit LinuxSystemOps(QDBusConnection bus, QObject *parent = nullptr);

    CommandResult run(const QString &program, const QStringList &args, const QHash<QString, QString> &extraEnv,
                      int timeoutMs) override;
    QString packageVersion(const QString &package) override;
    bool fileExists(const QString &path) override;
    FileOwnership stat(const QString &path) override;
    std::optional<QByteArray> readFile(const QString &path, qint64 maxBytes) override;
    bool writeFile(const QString &path, const QByteArray &data, unsigned uid, unsigned gid, unsigned mode,
                   QString *error) override;
    GroupInfo group(const QString &name) override;
    UserInfo user(const QString &name) override;
    QList<unsigned> supplementaryGroups(const QString &userName) override;
    QString unitLoadState(const QString &unit) override;
    void unitJob(const QString &method, const QString &unit, Completion done) override;
    bool setUnitFilesEnabled(const QStringList &units, bool enabled, QString *error) override;
    std::optional<QVariantMap> sessionProperties(const QString &sessionId) override;
    bool terminateSession(const QString &sessionId, QString *error) override;
    std::optional<QVariantMap> xrdpClientAddresses(QString *error) override;

private slots:
    void onJobRemoved(uint id, const QDBusObjectPath &job, const QString &unit, const QString &result);

private:
    struct PendingJob {
        QString unit;
        QString method;
        Completion done;
        QTimer *timer = nullptr;
    };
    void finishJob(const QString &jobPath, const QString &result);

    QDBusConnection m_bus;
    QHash<QString, PendingJob> m_jobs;
    QHash<QString, QString> m_earlyResults; // JobRemoved seen before the StartUnit reply
    bool m_subscribed = false;
};

} // namespace urdm::helper
