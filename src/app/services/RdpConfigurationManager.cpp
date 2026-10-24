#include "RdpConfigurationManager.h"

#include "ConfigStore.h"
#include "DBusTypes.h"
#include "HelperClient.h"
#include "ListenSpec.h"
#include "ProcessRunner.h"
#include "SettingsModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSslCertificate>
#include <QSysInfo>
#include <QUrl>

namespace urdm::app {

RdpConfigurationManager::RdpConfigurationManager(HelperClient *helper, Paths paths, QObject *parent)
    : QObject(parent)
    , m_helper(helper)
    , m_paths(std::move(paths))
    , m_settings(new SettingsModel(this, this))
{
    // Writers replace files by rename, so watch the directory, not the files.
    m_watcher.addPath(m_paths.configDir);
    if (QFileInfo::exists(m_paths.backupDir))
        m_watcher.addPath(m_paths.backupDir);
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(250);
    connect(&m_reloadDebounce, &QTimer::timeout, this, &RdpConfigurationManager::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() { m_reloadDebounce.start(); });
    reload();
    refreshVersions();
}

void RdpConfigurationManager::reload()
{
    QString err;
    auto loadOne = [&](const QString &name) -> std::optional<IniDocument> {
        QFile f(m_paths.configDir + QLatin1Char('/') + name);
        if (!f.open(QIODevice::ReadOnly)) {
            err += QStringLiteral("%1: %2. ").arg(f.fileName(), f.errorString());
            return std::nullopt;
        }
        return IniDocument::parse(f.read(4 * 1024 * 1024));
    };
    m_xrdp = loadOne(QStringLiteral("xrdp.ini"));
    m_sesman = loadOne(QStringLiteral("sesman.ini"));
    m_loadError = err.trimmed();
    if (!m_watcher.directories().contains(m_paths.configDir) && QFileInfo::exists(m_paths.configDir))
        m_watcher.addPath(m_paths.configDir);
    // re-validate pending edits against the new state
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
        const QString e = validate(it.key(), it.value());
        if (e.isEmpty())
            m_pendingErrors.remove(it.key());
        else
            m_pendingErrors.insert(it.key(), e);
    }
    loadCertificateInfo();
    loadDesktops();
    loadBackups();
    m_settings->refreshAll();
    emit loaded();
}

void RdpConfigurationManager::refreshVersions()
{
    auto query = [this](const QString &pkg, QString *target) {
        ProcessRunner::run(this, QStringLiteral("dpkg-query"),
                           {QStringLiteral("-W"), QStringLiteral("-f=${db:Status-Abbrev}|${Version}"), QStringLiteral("--"), pkg}, 15000,
                           [this, target, pkg](const ProcessResult &r) {
                               QString v;
                               const QString out = QString::fromUtf8(r.stdOut).trimmed();
                               if (r.ok() && out.startsWith(QLatin1String("ii")))
                                   v = out.section(QLatin1Char('|'), 1);
                               *target = v;
                               if (pkg == QLatin1String("xrdp"))
                                   m_version = XrdpVersion::fromDebianVersion(v);
                               m_settings->refreshAll();
                               emit versionChanged();
                               emit loaded();
                           });
    };
    query(QStringLiteral("xrdp"), &m_xrdpPackageVersion);
    query(QStringLiteral("xorgxrdp"), &m_xorgxrdpPackageVersion);
}

const IniDocument *RdpConfigurationManager::doc(ConfigFile f) const
{
    const auto &d = f == ConfigFile::XrdpIni ? m_xrdp : m_sesman;
    return d ? &*d : nullptr;
}

QString RdpConfigurationManager::value(const QString &id) const
{
    const SettingDef *d = ConfigSchema::find(id);
    if (!d)
        return {};
    const IniDocument *doc = this->doc(d->file);
    if (doc && doc->hasKey(d->section, d->key))
        return doc->value(d->section, d->key);
    return d->upstreamDefault;
}

bool RdpConfigurationManager::isDefault(const QString &id) const
{
    const SettingDef *d = ConfigSchema::find(id);
    const IniDocument *doc = d ? this->doc(d->file) : nullptr;
    return !doc || !doc->hasKey(d->section, d->key);
}

QString RdpConfigurationManager::rawSesmanValue(const QString &section, const QString &key) const
{
    return m_sesman ? m_sesman->value(section, key) : QString();
}

QString RdpConfigurationManager::pendingValue(const QString &id) const
{
    return m_pending.contains(id) ? m_pending.value(id) : value(id);
}

QString RdpConfigurationManager::validate(const QString &id, const QString &v) const
{
    const SettingDef *d = ConfigSchema::find(id);
    if (!d)
        return QStringLiteral("Unknown setting");
    if (auto e = ConfigSchema::validate(*d, v, m_version))
        return *e;
    return {};
}

QString RdpConfigurationManager::setPending(const QString &id, const QString &v)
{
    const SettingDef *d = ConfigSchema::find(id);
    if (!d || d->internalOnly)
        return QStringLiteral("This setting cannot be edited here");
    const QString err = validate(id, v);
    if (v.trimmed() == value(id).trimmed()) {
        m_pending.remove(id);
        m_pendingErrors.remove(id);
    } else {
        m_pending.insert(id, v.trimmed());
        if (err.isEmpty())
            m_pendingErrors.remove(id);
        else
            m_pendingErrors.insert(id, err);
    }
    m_settings->refreshRow(id);
    emit pendingChanged();
    return err;
}

void RdpConfigurationManager::revert(const QString &id)
{
    m_pending.remove(id);
    m_pendingErrors.remove(id);
    m_settings->refreshRow(id);
    emit pendingChanged();
}

void RdpConfigurationManager::discard()
{
    m_pending.clear();
    m_pendingErrors.clear();
    m_settings->refreshAll();
    emit pendingChanged();
}

void RdpConfigurationManager::apply()
{
    if (m_applying || m_pending.isEmpty())
        return;
    if (!m_pendingErrors.isEmpty()) {
        emit notification(QStringLiteral("error"), QStringLiteral("Some values are not valid. Please correct the ones marked in red."));
        return;
    }
    QHash<ConfigFile, QList<QPair<QString, QString>>> byFile;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it)
        byFile[ConfigSchema::find(it.key())->file] << qMakePair(it.key(), it.value());
    QList<QPair<ConfigFile, QList<QPair<QString, QString>>>> queue;
    for (auto it = byFile.cbegin(); it != byFile.cend(); ++it)
        queue << qMakePair(it.key(), it.value());
    m_applying = true;
    emit applyingChanged();
    applyNextFile(queue);
}

void RdpConfigurationManager::applyValue(const QString &id, const QString &v)
{
    const SettingDef *d = ConfigSchema::find(id);
    if (!d || d->internalOnly) {
        emit notification(QStringLiteral("error"), QStringLiteral("This setting cannot be changed here"));
        return;
    }
    if (const QString err = validate(id, v); !err.isEmpty()) {
        emit notification(QStringLiteral("error"), err);
        return;
    }
    const QList<DBusConfigChange> changes{{d->section, d->key, v.trimmed()}};
    helperOp(QStringLiteral("applyValue"), QStringLiteral("ApplyConfigChanges"), {configFileName(d->file), QVariant::fromValue(changes)}, true);
}

void RdpConfigurationManager::applyNextFile(QList<QPair<ConfigFile, QList<QPair<QString, QString>>>> queue)
{
    if (queue.isEmpty()) {
        m_applying = false;
        emit applyingChanged();
        setRestartRequired(true);
        reload();
        emit notification(QStringLiteral("success"), QStringLiteral("Settings saved. Restart Remote Desktop (Turn On / Off page) to start using them."));
        emit operationFinished(QStringLiteral("apply"), true, {});
        return;
    }
    const auto item = queue.takeFirst();
    QList<DBusConfigChange> changes;
    for (const auto &kv : item.second) {
        const SettingDef *d = ConfigSchema::find(kv.first);
        changes << DBusConfigChange{d->section, d->key, kv.second};
    }
    m_helper->call(QStringLiteral("ApplyConfigChanges"), {configFileName(item.first), QVariant::fromValue(changes)},
                   [this, item, queue](const HelperReply &r) {
                       if (!r.ok) {
                           m_applying = false;
                           emit applyingChanged();
                           reload();
                           emit notification(r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error"),
                                             r.notAuthorized ? r.message : QStringLiteral("The settings were not saved. Details: %1").arg(r.message));
                           emit operationFinished(QStringLiteral("apply"), false, r.message);
                           return;
                       }
                       for (const auto &kv : item.second) {
                           m_pending.remove(kv.first);
                           m_pendingErrors.remove(kv.first);
                       }
                       emit pendingChanged();
                       applyNextFile(queue);
                   });
}

void RdpConfigurationManager::setRestartRequired(bool r)
{
    if (r)
        m_appliedAtMs = QDateTime::currentMSecsSinceEpoch();
    if (r != m_restartRequired) {
        m_restartRequired = r;
        emit restartRequiredChanged();
    }
}

void RdpConfigurationManager::noteServiceStarted(qint64 activeEnterMs)
{
    if (m_restartRequired && activeEnterMs > m_appliedAtMs)
        setRestartRequired(false);
}

void RdpConfigurationManager::helperOp(const QString &opName, const QString &method, const QVariantList &args, bool needsRestart)
{
    m_applying = true;
    emit applyingChanged();
    m_helper->call(method, args, [this, opName, needsRestart](const HelperReply &r) {
        m_applying = false;
        emit applyingChanged();
        if (r.ok && needsRestart)
            setRestartRequired(true);
        reload();
        static const QHash<QString, QString> done{
            {QStringLiteral("sessionLimits"), QStringLiteral("Connection limits saved. Restart Remote Desktop to start using them.")},
            {QStringLiteral("applyValue"), QStringLiteral("Saved. Restart Remote Desktop to start using the change.")},
            {QStringLiteral("generateCertificate"), QStringLiteral("A new security certificate was created. Restart Remote Desktop to start using it.")},
            {QStringLiteral("installCertificate"), QStringLiteral("Your certificate was installed. Restart Remote Desktop to start using it.")},
            {QStringLiteral("configureDesktop"), QStringLiteral("Saved. People will get the chosen desktop the next time they connect.")},
            {QStringLiteral("restoreBackup"), QStringLiteral("The earlier settings were restored. Restart Remote Desktop to start using them.")},
        };
        const QString text = r.ok ? done.value(opName, r.message)
                           : r.notAuthorized ? r.message
                                             : QStringLiteral("That did not work. Details: %1").arg(r.message);
        emit notification(r.ok ? QStringLiteral("success") : (r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error")), text);
        emit operationFinished(opName, r.ok, r.message);
    });
}

// ---- session limits ---------------------------------------------------------------------------
QString RdpConfigurationManager::limitKind() const
{
    if (!m_sesman)
        return QStringLiteral("unknown"); // xrdp not installed / file unreadable: nothing to interpret
    const auto s = SessionLimitPolicy(m_version).interpret(m_sesman ? m_sesman->value(QStringLiteral("Sessions"), QStringLiteral("MaxSessions")) : QString());
    switch (s.kind) {
    case SessionLimitPolicy::Kind::Unlimited: return QStringLiteral("unlimited");
    case SessionLimitPolicy::Kind::Limited: return QStringLiteral("limited");
    case SessionLimitPolicy::Kind::DeniesAllLogins: return QStringLiteral("denies-all");
    case SessionLimitPolicy::Kind::Invalid: return QStringLiteral("invalid");
    }
    return QStringLiteral("invalid");
}

QString RdpConfigurationManager::limitDescription() const
{
    if (!m_sesman)
        return QStringLiteral("Remote Desktop is not installed yet.");
    const auto st = SessionLimitPolicy(m_version).interpret(m_sesman->value(QStringLiteral("Sessions"), QStringLiteral("MaxSessions")));
    switch (st.kind) {
    case SessionLimitPolicy::Kind::Unlimited:
        return QStringLiteral("No limit — as many people as this computer can handle can connect at the same time.");
    case SessionLimitPolicy::Kind::Limited:
        return QStringLiteral("Up to %1 people can be connected at the same time.").arg(st.limit);
    case SessionLimitPolicy::Kind::DeniesAllLogins:
        return QStringLiteral("Nobody can connect: the limit is set to 0, which this version of the remote desktop server treats as \"block everyone\".");
    case SessionLimitPolicy::Kind::Invalid:
        break;
    }
    return QStringLiteral("The connection limit setting is not valid (%1).").arg(st.description);
}

int RdpConfigurationManager::maxSessions() const
{
    return m_sesman ? m_sesman->value(QStringLiteral("Sessions"), QStringLiteral("MaxSessions")).toInt() : 0;
}

void RdpConfigurationManager::applySessionLimits(const QVariantMap &l)
{
    QString err;
    const SessionLimitPolicy policy(m_version);
    const auto max = policy.toConfigValue(l.value(QStringLiteral("unlimited")).toBool(), l.value(QStringLiteral("maxSessions")).toInt(), &err);
    if (!max) {
        emit notification(QStringLiteral("error"), err);
        return;
    }
    QList<DBusConfigChange> changes{{QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QString::number(*max)}};
    auto add = [&](const QString &key, const QString &id, const QVariant &v) {
        if (!v.isValid())
            return;
        const QString s = v.typeId() == QMetaType::Bool ? (v.toBool() ? QStringLiteral("true") : QStringLiteral("false")) : v.toString();
        if (const QString e = validate(id, s); !e.isEmpty())
            err = e;
        changes << DBusConfigChange{QStringLiteral("Sessions"), key, s};
    };
    add(QStringLiteral("Policy"), QStringLiteral("sessions.policy"), l.value(QStringLiteral("policy")));
    add(QStringLiteral("KillDisconnected"), QStringLiteral("sessions.killDisconnected"), l.value(QStringLiteral("killDisconnected")));
    add(QStringLiteral("DisconnectedTimeLimit"), QStringLiteral("sessions.disconnectedTimeLimit"), l.value(QStringLiteral("disconnectedTimeLimit")));
    add(QStringLiteral("IdleTimeLimit"), QStringLiteral("sessions.idleTimeLimit"), l.value(QStringLiteral("idleTimeLimit")));
    if (!err.isEmpty()) {
        emit notification(QStringLiteral("error"), err);
        return;
    }
    helperOp(QStringLiteral("sessionLimits"), QStringLiteral("ApplyConfigChanges"),
             {QStringLiteral("sesman.ini"), QVariant::fromValue(changes)}, true);
}

// ---- network / TLS ----------------------------------------------------------------------------
QVariantList RdpConfigurationManager::listenEndpoints() const
{
    QVariantList out;
    const auto spec = ListenSpec::parse(listenSpec());
    if (!spec)
        return out;
    for (const auto &ep : spec->endpoints())
        out << QVariantMap{{QStringLiteral("text"), ep.displayAddress()}, {QStringLiteral("port"), ep.port},
                           {QStringLiteral("loopback"), ep.isLoopbackOnly()}, {QStringLiteral("all"), ep.isAllInterfaces()}};
    return out;
}

QList<int> RdpConfigurationManager::configuredTcpPorts() const
{
    const auto spec = ListenSpec::parse(listenSpec());
    return spec ? spec->tcpPorts() : QList<int>{3389};
}

bool RdpConfigurationManager::exposesNonLoopback() const
{
    const auto spec = ListenSpec::parse(listenSpec());
    return spec && spec->exposesNonLoopback();
}

int RdpConfigurationManager::primaryPort() const
{
    const QList<int> p = configuredTcpPorts();
    return p.isEmpty() ? 3389 : p.first();
}

QString RdpConfigurationManager::makeListenSpec(const QString &address, int port) const
{
    return ListenSpec::singleTcp(address.trimmed(), port).toString();
}

QString RdpConfigurationManager::certificatePath() const
{
    const QString v = m_xrdp ? m_xrdp->value(QStringLiteral("Globals"), QStringLiteral("certificate")) : QString();
    return v.isEmpty() ? m_paths.configDir + QStringLiteral("/cert.pem") : v;
}

QString RdpConfigurationManager::keyPath() const
{
    const QString v = m_xrdp ? m_xrdp->value(QStringLiteral("Globals"), QStringLiteral("key_file")) : QString();
    return v.isEmpty() ? m_paths.configDir + QStringLiteral("/key.pem") : v;
}

void RdpConfigurationManager::loadCertificateInfo()
{
    QVariantMap info;
    const QString path = certificatePath();
    info.insert(QStringLiteral("path"), path);
    info.insert(QStringLiteral("resolvedPath"), QFileInfo(path).canonicalFilePath());
    const auto certs = QSslCertificate::fromPath(path, QSsl::Pem);
    if (certs.isEmpty()) {
        info.insert(QStringLiteral("error"), QFileInfo::exists(path) ? QStringLiteral("Not a readable PEM certificate")
                                                                    : QStringLiteral("File does not exist"));
    } else {
        const QSslCertificate &c = certs.first();
        const QDateTime now = QDateTime::currentDateTimeUtc();
        info.insert(QStringLiteral("subject"), c.subjectInfo(QSslCertificate::CommonName).join(QStringLiteral(", ")));
        info.insert(QStringLiteral("issuer"), c.issuerInfo(QSslCertificate::CommonName).join(QStringLiteral(", ")));
        info.insert(QStringLiteral("notBefore"), c.effectiveDate().toString(Qt::ISODate));
        info.insert(QStringLiteral("notAfter"), c.expiryDate().toString(Qt::ISODate));
        info.insert(QStringLiteral("daysLeft"), now.daysTo(c.expiryDate()));
        info.insert(QStringLiteral("selfSigned"), c.isSelfSigned());
        info.insert(QStringLiteral("expired"), c.expiryDate() < now);
        QStringList sans;
        const auto alt = c.subjectAlternativeNames();
        for (auto it = alt.cbegin(); it != alt.cend(); ++it)
            sans << it.value();
        info.insert(QStringLiteral("altNames"), sans);
        info.insert(QStringLiteral("sha256"), QString::fromLatin1(c.digest(QCryptographicHash::Sha256).toHex(':')));
    }
    m_certInfo = info;
}

void RdpConfigurationManager::generateCertificate(const QString &commonName, int days)
{
    helperOp(QStringLiteral("generateCertificate"), QStringLiteral("GenerateSelfSignedCertificate"), {commonName.trimmed(), days}, true);
}

void RdpConfigurationManager::installCertificate(const QString &certPathOrUrl, const QString &keyPathOrUrl)
{
    auto toPath = [](const QString &s) {
        const QUrl u(s);
        return u.isLocalFile() ? u.toLocalFile() : s;
    };
    helperOp(QStringLiteral("installCertificate"), QStringLiteral("InstallCertificate"), {toPath(certPathOrUrl), toPath(keyPathOrUrl)}, true);
}

// ---- desktop ----------------------------------------------------------------------------------
QString RdpConfigurationManager::desktopId() const
{
    const QString wm = desktopScript();
    if (wm.isEmpty() || wm == QLatin1String("startwm.sh"))
        return QStringLiteral("system-default");
    if (wm.endsWith(QLatin1String("/startwm-xfce.sh")))
        return QStringLiteral("xfce");
    return QStringLiteral("custom");
}

void RdpConfigurationManager::loadDesktops()
{
    QStringList list;
    const QDir dir(m_paths.xsessionsDir);
    for (const QString &f : dir.entryList({QStringLiteral("*.desktop")}, QDir::Files)) {
        QSettings desk(dir.filePath(f), QSettings::IniFormat);
        const QString name = desk.value(QStringLiteral("Desktop Entry/Name")).toString();
        list << (name.isEmpty() ? f.chopped(8) : name);
    }
    m_desktops = list;
}

void RdpConfigurationManager::configureDesktop(const QString &id)
{
    helperOp(QStringLiteral("configureDesktop"), QStringLiteral("ConfigureDesktop"), {id}, false);
}

QString RdpConfigurationManager::accessGroup() const
{
    return m_sesman ? m_sesman->value(QStringLiteral("Security"), QStringLiteral("TerminalServerUsers")) : QString();
}

// ---- backups ----------------------------------------------------------------------------------
void RdpConfigurationManager::loadBackups()
{
    QVariantList list;
    ConfigStore store(m_paths.configDir, m_paths.backupDir);
    for (ConfigFile f : {ConfigFile::SesmanIni, ConfigFile::XrdpIni})
        for (const BackupInfo &b : store.listBackups(f))
            list << QVariantMap{{QStringLiteral("id"), b.id}, {QStringLiteral("file"), configFileName(f)},
                                {QStringLiteral("created"), b.created.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))},
                                {QStringLiteral("createdMs"), b.created.toMSecsSinceEpoch()}};
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("createdMs")).toLongLong() > b.toMap().value(QStringLiteral("createdMs")).toLongLong();
    });
    if (!m_watcher.directories().contains(m_paths.backupDir) && QFileInfo::exists(m_paths.backupDir))
        m_watcher.addPath(m_paths.backupDir);
    m_backups = list;
    emit backupsChanged();
}

void RdpConfigurationManager::restoreBackup(const QString &file, const QString &backupId)
{
    helperOp(QStringLiteral("restoreBackup"), QStringLiteral("RestoreBackup"), {file, backupId}, true);
}

QString RdpConfigurationManager::hostName() const
{
    return QSysInfo::machineHostName();
}

} // namespace urdm::app
