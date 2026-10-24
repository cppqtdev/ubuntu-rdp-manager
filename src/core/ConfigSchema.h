#pragma once

#include "XrdpVersion.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace urdm {

enum class ConfigFile { XrdpIni, SesmanIni };

QString configFileName(ConfigFile file);                 // "xrdp.ini" / "sesman.ini"
std::optional<ConfigFile> configFileFromName(const QString &name);

// One administrator-editable setting. Only settings listed here can be written by the helper;
// each was verified against the xrdp 0.9.24 man pages and source (docs/configuration.md).
struct SettingDef
{
    enum class Type { Bool, Int, Enum, EnumList, String, Path, ListenSpec, GroupName, CipherList, TlsProtocols, MaxSessions };

    QString id;            // stable identifier used by the UI, e.g. "sessions.maxSessions"
    ConfigFile file;
    QString section;
    QString key;
    Type type;
    QString label;
    QString help;
    QString category;      // UI grouping: "Network", "Security", "Sessions", "Redirection", "Logging", "Desktop"
    QStringList choices;   // Enum / EnumList (values written to the file)
    QStringList choiceLabels; // plain-language names shown for `choices` (same order)
    int minimum = 0;
    int maximum = 0;
    QString upstreamDefault; // value sesman/xrdp uses when the key is absent
    bool allowEmpty = false;
    bool internalOnly = false; // writable only by dedicated helper operations, not generic edits
    XrdpVersion minVersion{0, 9, 0, true};
};

// Value normalisation for booleans as accepted by xrdp's g_text2bool ("1", "true", "yes").
bool parseXrdpBool(const QString &value);

class ConfigSchema
{
public:
    static const QList<SettingDef> &settings();
    static const SettingDef *find(const QString &id);
    static const SettingDef *find(ConfigFile file, const QString &section, const QString &key);

    // Returns an error message, or std::nullopt when the value is acceptable for the version.
    // Enforces secure-default rules (no legacy TLS protocols, no plaintext RDP-only security).
    static std::optional<QString> validate(const SettingDef &def, const QString &value, const XrdpVersion &version);
};

} // namespace urdm
