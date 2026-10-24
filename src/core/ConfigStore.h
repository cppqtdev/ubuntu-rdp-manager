#pragma once

#include "ConfigSchema.h"
#include "IniDocument.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

namespace urdm {

struct ConfigChange
{
    QString section;
    QString key;
    QString value;
    bool remove = false; // delete the key instead of setting it
};

struct BackupInfo
{
    QString id;        // e.g. "sesman.ini.20261024T101530123"
    QString path;
    QDateTime created;
    ConfigFile file;
};

// File-level configuration management for /etc/xrdp. Every write:
//   1. validates each change against ConfigSchema for the installed xrdp version,
//   2. copies the current file into the backup directory (keeping mode bits),
//   3. writes the new content atomically (QSaveFile: temp file + fsync + rename),
//   4. re-reads and re-validates the written file, restoring the backup on mismatch.
class ConfigStore
{
public:
    ConfigStore(QString configDir, QString backupDir, int maxBackupsPerFile = 30);

    QString filePath(ConfigFile file) const;
    std::optional<IniDocument> load(ConfigFile file, QString *error = nullptr) const;

    // Pure: validates and applies the changes to a document without touching disk.
    // allowInternal permits internalOnly settings (used by dedicated helper operations).
    static bool applyChanges(IniDocument &doc, ConfigFile file, const QList<ConfigChange> &changes,
                             const XrdpVersion &version, bool allowInternal, QString *error);

    // Validates, backs up, writes. Returns the backup id on success.
    std::optional<QString> write(ConfigFile file, const QList<ConfigChange> &changes, const XrdpVersion &version,
                                 bool allowInternal, QString *error);

    QList<BackupInfo> listBackups(ConfigFile file) const;
    // Restores a backup. The current file is backed up first so the restore can be undone.
    bool restore(ConfigFile file, const QString &backupId, QString *error);

private:
    bool restoreRaw(ConfigFile file, const QString &backupId, QString *error);
    std::optional<QString> createBackup(ConfigFile file, QString *error);
    bool atomicWrite(const QString &path, const QByteArray &data, QString *error);
    void pruneBackups(ConfigFile file);

    QString m_configDir;
    QString m_backupDir;
    int m_maxBackups;
};

} // namespace urdm
