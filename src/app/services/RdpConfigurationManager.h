#pragma once

#include "ConfigSchema.h"
#include "IniDocument.h"
#include "SessionLimitPolicy.h"
#include "SettingsModel.h"
#include "XrdpVersion.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <optional>

namespace urdm::app {

class HelperClient;

// Reads /etc/xrdp/{xrdp,sesman}.ini directly (world-readable), tracks edits, and applies them
// through the helper which validates, backs up and writes atomically.
class RdpConfigurationManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(SettingsModel *settings READ settings CONSTANT)
    Q_PROPERTY(bool configPresent READ configPresent NOTIFY loaded)
    Q_PROPERTY(QString loadError READ loadError NOTIFY loaded)
    Q_PROPERTY(QString xrdpVersion READ xrdpVersionText NOTIFY versionChanged)
    Q_PROPERTY(QString xrdpPackageVersion READ xrdpPackageVersion NOTIFY versionChanged)
    Q_PROPERTY(bool xrdpInstalled READ xrdpInstalled NOTIFY versionChanged)
    Q_PROPERTY(QString xorgxrdpPackageVersion READ xorgxrdpPackageVersion NOTIFY versionChanged)
    Q_PROPERTY(bool zeroMeansUnlimited READ zeroMeansUnlimited NOTIFY versionChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingChanged)
    Q_PROPERTY(bool pendingValid READ pendingValid NOTIFY pendingChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)
    Q_PROPERTY(QString limitKind READ limitKind NOTIFY loaded)
    Q_PROPERTY(QString limitDescription READ limitDescription NOTIFY loaded)
    Q_PROPERTY(int maxSessions READ maxSessions NOTIFY loaded)
    Q_PROPERTY(QString policy READ policy NOTIFY loaded)
    Q_PROPERTY(bool killDisconnected READ killDisconnected NOTIFY loaded)
    Q_PROPERTY(int disconnectedTimeLimit READ disconnectedTimeLimit NOTIFY loaded)
    Q_PROPERTY(int idleTimeLimit READ idleTimeLimit NOTIFY loaded)
    Q_PROPERTY(QString listenSpec READ listenSpec NOTIFY loaded)
    Q_PROPERTY(QVariantList listenEndpoints READ listenEndpoints NOTIFY loaded)
    Q_PROPERTY(bool exposesNonLoopback READ exposesNonLoopback NOTIFY loaded)
    Q_PROPERTY(int primaryPort READ primaryPort NOTIFY loaded)
    Q_PROPERTY(QString securityLayer READ securityLayer NOTIFY loaded)
    Q_PROPERTY(QString certificatePath READ certificatePath NOTIFY loaded)
    Q_PROPERTY(QString keyPath READ keyPath NOTIFY loaded)
    Q_PROPERTY(QVariantMap certificateInfo READ certificateInfo NOTIFY loaded)
    Q_PROPERTY(QString desktopId READ desktopId NOTIFY loaded)
    Q_PROPERTY(QString desktopScript READ desktopScript NOTIFY loaded)
    Q_PROPERTY(QStringList installedDesktops READ installedDesktops NOTIFY loaded)
    Q_PROPERTY(QString accessGroup READ accessGroup NOTIFY loaded)
    Q_PROPERTY(bool alwaysGroupCheck READ alwaysGroupCheck NOTIFY loaded)
    Q_PROPERTY(bool allowRootLogin READ allowRootLogin NOTIFY loaded)
    Q_PROPERTY(QVariantList backups READ backups NOTIFY backupsChanged)
    Q_PROPERTY(QString hostName READ hostName CONSTANT)

public:
    struct Paths {
        QString configDir = QStringLiteral("/etc/xrdp");
        QString backupDir = QStringLiteral("/var/lib/ubuntu-rdp-manager/backups");
        QString xsessionsDir = QStringLiteral("/usr/share/xsessions");
    };
    RdpConfigurationManager(HelperClient *helper, Paths paths, QObject *parent = nullptr);

    SettingsModel *settings() const { return m_settings; }
    bool configPresent() const { return m_xrdp.has_value() && m_sesman.has_value(); }
    QString loadError() const { return m_loadError; }
    XrdpVersion xrdpVersion() const { return m_version; }
    QString xrdpVersionText() const { return m_version.valid ? m_version.toString() : QString(); }
    QString xrdpPackageVersion() const { return m_xrdpPackageVersion; }
    bool xrdpInstalled() const { return !m_xrdpPackageVersion.isEmpty(); }
    QString xorgxrdpPackageVersion() const { return m_xorgxrdpPackageVersion; }
    bool zeroMeansUnlimited() const { return SessionLimitPolicy(m_version).zeroMeansUnlimited(); }

    // generic settings access (used by SettingsModel and QML)
    Q_INVOKABLE QString value(const QString &id) const;      // effective value (file or upstream default)
    Q_INVOKABLE QString pendingValue(const QString &id) const;
    Q_INVOKABLE bool isModified(const QString &id) const { return m_pending.contains(id); }
    bool isDefault(const QString &id) const;
    QString pendingError(const QString &id) const { return m_pendingErrors.value(id); }
    Q_INVOKABLE QString setPending(const QString &id, const QString &value); // returns validation error or ""
    Q_INVOKABLE void revert(const QString &id);
    Q_INVOKABLE void discard();
    Q_INVOKABLE void apply();
    // Applies one setting immediately, independent of pending edits.
    Q_INVOKABLE void applyValue(const QString &id, const QString &value);
    Q_INVOKABLE QString validate(const QString &id, const QString &value) const;
    int pendingCount() const { return int(m_pending.size()); }
    bool pendingValid() const { return m_pendingErrors.isEmpty(); }
    bool applying() const { return m_applying; }
    bool restartRequired() const { return m_restartRequired; }
    void noteServiceStarted(qint64 activeEnterMs);

    // session limits
    QString limitKind() const;
    QString limitDescription() const;
    int maxSessions() const;
    QString policy() const { return value(QStringLiteral("sessions.policy")); }
    bool killDisconnected() const { return parseXrdpBool(value(QStringLiteral("sessions.killDisconnected"))); }
    int disconnectedTimeLimit() const { return value(QStringLiteral("sessions.disconnectedTimeLimit")).toInt(); }
    int idleTimeLimit() const { return value(QStringLiteral("sessions.idleTimeLimit")).toInt(); }
    Q_INVOKABLE void applySessionLimits(const QVariantMap &limits);
    Q_INVOKABLE int unlimitedConfigValue() const { return SessionLimitPolicy(m_version).unlimitedConfigValue(); }

    // network / TLS
    QString listenSpec() const { return value(QStringLiteral("network.listen")); }
    QVariantList listenEndpoints() const;
    QList<int> configuredTcpPorts() const;
    bool exposesNonLoopback() const;
    int primaryPort() const;
    Q_INVOKABLE QString makeListenSpec(const QString &address, int port) const;
    QString securityLayer() const { return value(QStringLiteral("security.layer")); }
    QString certificatePath() const;
    QString keyPath() const;
    QVariantMap certificateInfo() const { return m_certInfo; }
    Q_INVOKABLE void generateCertificate(const QString &commonName, int days);
    Q_INVOKABLE void installCertificate(const QString &certPathOrUrl, const QString &keyPathOrUrl);

    // desktop
    QString desktopId() const;
    QString desktopScript() const { return value(QStringLiteral("desktop.defaultWindowManager")); }
    QStringList installedDesktops() const { return m_desktops; }
    Q_INVOKABLE void configureDesktop(const QString &desktopId);

    // access (read-only here; changed through UserAccessManager)
    QString accessGroup() const;
    bool alwaysGroupCheck() const { return parseXrdpBool(value(QStringLiteral("access.alwaysGroupCheck"))); }
    bool allowRootLogin() const { return parseXrdpBool(value(QStringLiteral("security.allowRootLogin"))); }

    // backups
    QVariantList backups() const { return m_backups; }
    Q_INVOKABLE void restoreBackup(const QString &file, const QString &backupId);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void refreshVersions();

    QString hostName() const;
    QString rawSesmanValue(const QString &section, const QString &key) const;

signals:
    void loaded();
    void versionChanged();
    void pendingChanged();
    void applyingChanged();
    void restartRequiredChanged();
    void backupsChanged();
    void notification(const QString &level, const QString &text);
    void operationFinished(const QString &operation, bool ok, const QString &message);

private:
    const IniDocument *doc(ConfigFile f) const;
    void loadCertificateInfo();
    void loadBackups();
    void loadDesktops();
    void helperOp(const QString &opName, const QString &method, const QVariantList &args, bool needsRestart);
    void applyNextFile(QList<QPair<ConfigFile, QList<QPair<QString, QString>>>> queue);
    void setRestartRequired(bool r);

    HelperClient *m_helper;
    Paths m_paths;
    SettingsModel *m_settings;
    std::optional<IniDocument> m_xrdp;
    std::optional<IniDocument> m_sesman;
    QString m_loadError;
    XrdpVersion m_version;
    QString m_xrdpPackageVersion;
    QString m_xorgxrdpPackageVersion;
    QHash<QString, QString> m_pending;
    QHash<QString, QString> m_pendingErrors;
    bool m_applying = false;
    bool m_restartRequired = false;
    qint64 m_appliedAtMs = 0;
    QVariantMap m_certInfo;
    QStringList m_desktops;
    QVariantList m_backups;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadDebounce;
};

} // namespace urdm::app
