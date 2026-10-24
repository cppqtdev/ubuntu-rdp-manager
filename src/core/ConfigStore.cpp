#include "ConfigStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace urdm {

namespace {
const QString kTimestampFormat = QStringLiteral("yyyyMMdd'T'HHmmsszzz");
}

ConfigStore::ConfigStore(QString configDir, QString backupDir, int maxBackupsPerFile)
    : m_configDir(std::move(configDir))
    , m_backupDir(std::move(backupDir))
    , m_maxBackups(maxBackupsPerFile)
{
}

QString ConfigStore::filePath(ConfigFile file) const
{
    return m_configDir + QLatin1Char('/') + configFileName(file);
}

std::optional<IniDocument> ConfigStore::load(ConfigFile file, QString *error) const
{
    QFile f(filePath(file));
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot read %1: %2").arg(f.fileName(), f.errorString());
        return std::nullopt;
    }
    if (f.size() > 4 * 1024 * 1024) {
        if (error)
            *error = QStringLiteral("%1 is unexpectedly large").arg(f.fileName());
        return std::nullopt;
    }
    return IniDocument::parse(f.readAll());
}

bool ConfigStore::applyChanges(IniDocument &doc, ConfigFile file, const QList<ConfigChange> &changes,
                               const XrdpVersion &version, bool allowInternal, QString *error)
{
    if (changes.isEmpty()) {
        if (error)
            *error = QStringLiteral("No changes requested");
        return false;
    }
    for (const ConfigChange &c : changes) {
        const SettingDef *d = ConfigSchema::find(file, c.section, c.key);
        if (!d) {
            if (error)
                *error = QStringLiteral("[%1] %2 in %3 is not a supported setting").arg(c.section, c.key, configFileName(file));
            return false;
        }
        if (d->internalOnly && !allowInternal) {
            if (error)
                *error = QStringLiteral("%1 can only be changed through its dedicated operation").arg(d->label);
            return false;
        }
        if (c.remove) {
            doc.removeKey(d->section, d->key);
            continue;
        }
        if (auto err = ConfigSchema::validate(*d, c.value, version)) {
            if (error)
                *error = *err;
            return false;
        }
        // Write using the schema's canonical section/key spelling only when absent; setValue
        // keeps the existing spelling otherwise.
        doc.setValue(d->section, d->key, c.value.trimmed());
    }
    return true;
}

std::optional<QString> ConfigStore::write(ConfigFile file, const QList<ConfigChange> &changes,
                                          const XrdpVersion &version, bool allowInternal, QString *error)
{
    auto doc = load(file, error);
    if (!doc)
        return std::nullopt;
    const QByteArray before = doc->serialize();
    if (!applyChanges(*doc, file, changes, version, allowInternal, error))
        return std::nullopt;
    const QByteArray after = doc->serialize();

    auto backupId = createBackup(file, error);
    if (!backupId)
        return std::nullopt;

    if (after == before)
        return backupId; // nothing to change; backup still documents the state

    const QString path = filePath(file);
    if (!atomicWrite(path, after, error)) {
        restoreRaw(file, *backupId, nullptr);
        return std::nullopt;
    }

    // Verify: what is on disk must parse to the values we intended.
    QString verifyError;
    auto check = load(file, &verifyError);
    bool ok = check.has_value();
    if (ok) {
        for (const auto &c : changes) {
            const SettingDef *d = ConfigSchema::find(file, c.section, c.key);
            const bool mismatch = c.remove ? check->hasKey(d->section, d->key)
                                           : check->value(d->section, d->key) != c.value.trimmed();
            if (mismatch) {
                ok = false;
                verifyError = QStringLiteral("Verification failed for %1").arg(d->key);
                break;
            }
        }
    }
    if (!ok) {
        QString restoreError;
        restoreRaw(file, *backupId, &restoreError);
        if (error)
            *error = verifyError + (restoreError.isEmpty() ? QStringLiteral("; previous file restored")
                                                           : QStringLiteral("; RESTORE FAILED: ") + restoreError);
        return std::nullopt;
    }
    pruneBackups(file);
    return backupId;
}

std::optional<QString> ConfigStore::createBackup(ConfigFile file, QString *error)
{
    if (!QDir().mkpath(m_backupDir)) {
        if (error)
            *error = QStringLiteral("Cannot create backup directory %1").arg(m_backupDir);
        return std::nullopt;
    }
    const QString src = filePath(file);
    QString id = configFileName(file) + QLatin1Char('.') + QDateTime::currentDateTimeUtc().toString(kTimestampFormat);
    QString dst = m_backupDir + QLatin1Char('/') + id;
    for (int n = 1; QFile::exists(dst); ++n) { // same millisecond
        id += QString::number(n);
        dst = m_backupDir + QLatin1Char('/') + id;
    }
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot read %1 for backup: %2").arg(src, in.errorString());
        return std::nullopt;
    }
    const QByteArray data = in.readAll();
    if (!atomicWrite(dst, data, error))
        return std::nullopt;
    QFile::setPermissions(dst, in.permissions());
    return id;
}

bool ConfigStore::atomicWrite(const QString &path, const QByteArray &data, QString *error)
{
    const QFileInfo existing(path);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open %1 for writing: %2").arg(path, out.errorString());
        return false;
    }
    if (out.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("Short write to %1: %2").arg(path, out.errorString());
        out.cancelWriting();
        return false;
    }
    if (existing.exists())
        out.setPermissions(existing.permissions());
    if (!out.commit()) {
        if (error)
            *error = QStringLiteral("Cannot commit %1: %2").arg(path, out.errorString());
        return false;
    }
    return true;
}

QList<BackupInfo> ConfigStore::listBackups(ConfigFile file) const
{
    QList<BackupInfo> result;
    const QString prefix = configFileName(file) + QLatin1Char('.');
    const QDir dir(m_backupDir);
    const QStringList names = dir.entryList({prefix + QLatin1Char('*')}, QDir::Files, QDir::Name | QDir::Reversed);
    static const QRegularExpression re(QStringLiteral(R"(^(\d{8}T\d{9})\d*$)"));
    for (const QString &name : names) {
        const auto m = re.match(name.mid(prefix.size()));
        if (!m.hasMatch())
            continue;
        BackupInfo b;
        b.id = name;
        b.path = dir.filePath(name);
        b.created = QDateTime::fromString(m.captured(1), kTimestampFormat);
        b.created.setTimeZone(QTimeZone::UTC);
        b.file = file;
        result << b;
    }
    return result;
}

bool ConfigStore::restore(ConfigFile file, const QString &backupId, QString *error)
{
    if (!QFile::exists(m_backupDir + QLatin1Char('/') + backupId) || backupId.contains(QLatin1Char('/'))) {
        if (error)
            *error = QStringLiteral("Backup %1 not found").arg(backupId);
        return false;
    }
    if (!createBackup(file, error))
        return false;
    const bool ok = restoreRaw(file, backupId, error);
    pruneBackups(file);
    return ok;
}

bool ConfigStore::restoreRaw(ConfigFile file, const QString &backupId, QString *error)
{
    // backupId must name a file inside the backup directory for this config file only.
    const QString prefix = configFileName(file) + QLatin1Char('.');
    static const QRegularExpression idRe(QStringLiteral(R"(^(xrdp|sesman)\.ini\.\d{8}T\d{9}\d*$)"));
    if (!backupId.startsWith(prefix) || !idRe.match(backupId).hasMatch()) {
        if (error)
            *error = QStringLiteral("Invalid backup identifier");
        return false;
    }
    QFile in(m_backupDir + QLatin1Char('/') + backupId);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Backup %1 not found").arg(backupId);
        return false;
    }
    return atomicWrite(filePath(file), in.readAll(), error);
}

void ConfigStore::pruneBackups(ConfigFile file)
{
    const auto backups = listBackups(file);
    for (qsizetype i = m_maxBackups; i < backups.size(); ++i)
        QFile::remove(backups.at(i).path);
}

} // namespace urdm
