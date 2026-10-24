#include "ConfigSchema.h"
#include "ListenSpec.h"
#include "SessionLimitPolicy.h"
#include "Validators.h"

#include <QRegularExpression>

namespace urdm {

QString configFileName(ConfigFile file)
{
    return file == ConfigFile::XrdpIni ? QStringLiteral("xrdp.ini") : QStringLiteral("sesman.ini");
}

std::optional<ConfigFile> configFileFromName(const QString &name)
{
    if (name == QLatin1String("xrdp.ini"))
        return ConfigFile::XrdpIni;
    if (name == QLatin1String("sesman.ini"))
        return ConfigFile::SesmanIni;
    return std::nullopt;
}

bool parseXrdpBool(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes");
}

namespace {

using T = SettingDef::Type;

SettingDef def(QString id, ConfigFile f, QString section, QString key, T type, QString category,
               QString label, QString help, QString upstreamDefault = {})
{
    SettingDef d;
    d.id = std::move(id);
    d.file = f;
    d.section = std::move(section);
    d.key = std::move(key);
    d.type = type;
    d.category = std::move(category);
    d.label = std::move(label);
    d.help = std::move(help);
    d.upstreamDefault = std::move(upstreamDefault);
    return d;
}

QList<SettingDef> buildSettings()
{
    const auto X = ConfigFile::XrdpIni;
    const auto S = ConfigFile::SesmanIni;
    const QStringList logLevels{QStringLiteral("CORE"), QStringLiteral("ERROR"), QStringLiteral("WARNING"),
                                QStringLiteral("INFO"), QStringLiteral("DEBUG")};
    QList<SettingDef> l;

    // ---- Network -------------------------------------------------------------------------
    l << def(QStringLiteral("network.listen"), X, QStringLiteral("Globals"), QStringLiteral("port"), T::ListenSpec,
             QStringLiteral("Network"), QStringLiteral("Listening address and port"),
             QStringLiteral("Where xrdp accepts RDP connections. A bare number listens on all interfaces. "
                            "Prefer a specific LAN/VPN address; do not expose RDP directly to the internet."),
             QStringLiteral("3389"));
    l << def(QStringLiteral("network.tcpKeepalive"), X, QStringLiteral("Globals"), QStringLiteral("tcp_keepalive"),
             T::Bool, QStringLiteral("Network"), QStringLiteral("TCP keep-alive"),
             QStringLiteral("Detect vanished clients so their connection is closed and the session becomes disconnected."),
             QStringLiteral("false"));

    // ---- Security / TLS --------------------------------------------------------------------
    {
        auto d = def(QStringLiteral("security.layer"), X, QStringLiteral("Globals"), QStringLiteral("security_layer"),
                     T::Enum, QStringLiteral("Security"), QStringLiteral("Security layer"),
                     QStringLiteral("tls: TLS only (recommended). negotiate: TLS when the client supports it. "
                                    "rdp: legacy Standard RDP Security, vulnerable to man-in-the-middle; not allowed by this tool."),
                     QStringLiteral("negotiate"));
        d.choices = {QStringLiteral("tls"), QStringLiteral("negotiate"), QStringLiteral("rdp")};
        l << d;
    }
    {
        auto d = def(QStringLiteral("security.cryptLevel"), X, QStringLiteral("Globals"), QStringLiteral("crypt_level"),
                     T::Enum, QStringLiteral("Security"), QStringLiteral("Standard RDP encryption level"),
                     QStringLiteral("Only used by Standard RDP Security (negotiate fallback)."), QStringLiteral("high"));
        d.choices = {QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"), QStringLiteral("fips")};
        l << d;
    }
    {
        auto d = def(QStringLiteral("security.certificate"), X, QStringLiteral("Globals"), QStringLiteral("certificate"),
                     T::Path, QStringLiteral("Security"), QStringLiteral("TLS certificate"),
                     QStringLiteral("PEM certificate presented to clients. Empty uses /etc/xrdp/cert.pem."));
        d.allowEmpty = true;
        d.internalOnly = true; // set via InstallCertificate / GenerateSelfSignedCertificate
        l << d;
    }
    {
        auto d = def(QStringLiteral("security.keyFile"), X, QStringLiteral("Globals"), QStringLiteral("key_file"),
                     T::Path, QStringLiteral("Security"), QStringLiteral("TLS private key"),
                     QStringLiteral("PEM private key. Must be readable by the xrdp user only (root:xrdp 0640)."));
        d.allowEmpty = true;
        d.internalOnly = true;
        l << d;
    }
    l << def(QStringLiteral("security.tlsProtocols"), X, QStringLiteral("Globals"), QStringLiteral("ssl_protocols"),
             T::TlsProtocols, QStringLiteral("Security"), QStringLiteral("TLS protocol versions"),
             QStringLiteral("Comma separated. Only TLSv1.2 and TLSv1.3 may be enabled by this tool."),
             QStringLiteral("TLSv1.2, TLSv1.3"));
    {
        auto d = def(QStringLiteral("security.tlsCiphers"), X, QStringLiteral("Globals"), QStringLiteral("tls_ciphers"),
                     T::CipherList, QStringLiteral("Security"), QStringLiteral("TLS cipher list"),
                     QStringLiteral("OpenSSL cipher string. Leave empty for the OpenSSL default (recommended)."));
        d.allowEmpty = true;
        l << d;
    }
    l << def(QStringLiteral("security.allowRootLogin"), S, QStringLiteral("Security"), QStringLiteral("AllowRootLogin"),
             T::Bool, QStringLiteral("Security"), QStringLiteral("Allow root login"),
             QStringLiteral("Permit the root account to open a remote desktop. Disabling is strongly recommended."),
             QStringLiteral("false"));
    {
        auto d = def(QStringLiteral("security.maxLoginRetry"), S, QStringLiteral("Security"), QStringLiteral("MaxLoginRetry"),
                     T::Int, QStringLiteral("Security"), QStringLiteral("Maximum login attempts"),
                     QStringLiteral("Login attempts allowed per connection before it is dropped. 0 = unlimited."),
                     QStringLiteral("3"));
        d.minimum = 0;
        d.maximum = 100;
        l << d;
    }
    {
        auto d = def(QStringLiteral("access.group"), S, QStringLiteral("Security"), QStringLiteral("TerminalServerUsers"),
                     T::GroupName, QStringLiteral("Security"), QStringLiteral("RDP users group"),
                     QStringLiteral("Only members of this group may log in, provided the group exists (or AlwaysGroupCheck is on)."));
        d.internalOnly = true; // managed by SetAccessRestriction
        l << d;
    }
    {
        auto d = def(QStringLiteral("access.alwaysGroupCheck"), S, QStringLiteral("Security"), QStringLiteral("AlwaysGroupCheck"),
                     T::Bool, QStringLiteral("Security"), QStringLiteral("Always check group"),
                     QStringLiteral("Deny every login when the RDP users group does not exist (fail closed)."),
                     QStringLiteral("false"));
        d.internalOnly = true;
        l << d;
    }

    // ---- Sessions --------------------------------------------------------------------------
    l << def(QStringLiteral("sessions.maxSessions"), S, QStringLiteral("Sessions"), QStringLiteral("MaxSessions"),
             T::MaxSessions, QStringLiteral("Sessions"), QStringLiteral("Maximum sessions"),
             QStringLiteral("Configured upper bound on concurrent sessions. Hardware sets the real limit."),
             QStringLiteral("0"));
    {
        auto d = def(QStringLiteral("sessions.policy"), S, QStringLiteral("Sessions"), QStringLiteral("Policy"), T::Enum,
                     QStringLiteral("Sessions"), QStringLiteral("Session allocation policy"),
                     QStringLiteral("Default: reconnect to the user's existing session (per user and colour depth). "
                                    "UBC: a new session for every connection (same user may hold several desktops; "
                                    "reconnecting never reattaches). UBD/UBI/UBDI/UBDC also match display size / client IP."),
                     QStringLiteral("Default"));
        d.choices = {QStringLiteral("Default"), QStringLiteral("UBD"), QStringLiteral("UBI"), QStringLiteral("UBC"),
                     QStringLiteral("UBDI"), QStringLiteral("UBDC")};
        l << d;
    }
    l << def(QStringLiteral("sessions.killDisconnected"), S, QStringLiteral("Sessions"), QStringLiteral("KillDisconnected"),
             T::Bool, QStringLiteral("Sessions"), QStringLiteral("End disconnected sessions"),
             QStringLiteral("Terminate a session once it has been disconnected for the disconnected time limit."),
             QStringLiteral("false"));
    {
        auto d = def(QStringLiteral("sessions.disconnectedTimeLimit"), S, QStringLiteral("Sessions"),
                     QStringLiteral("DisconnectedTimeLimit"), T::Int, QStringLiteral("Sessions"),
                     QStringLiteral("Disconnected time limit (seconds)"),
                     QStringLiteral("0 = never. Used together with 'End disconnected sessions'."), QStringLiteral("0"));
        d.minimum = 0;
        d.maximum = 30 * 24 * 3600;
        l << d;
    }
    {
        auto d = def(QStringLiteral("sessions.idleTimeLimit"), S, QStringLiteral("Sessions"), QStringLiteral("IdleTimeLimit"),
                     T::Int, QStringLiteral("Sessions"), QStringLiteral("Idle time limit (seconds)"),
                     QStringLiteral("Disconnect a connected session after this many seconds without input. 0 = never."),
                     QStringLiteral("0"));
        d.minimum = 0;
        d.maximum = 30 * 24 * 3600;
        l << d;
    }
    {
        auto d = def(QStringLiteral("sessions.displayOffset"), S, QStringLiteral("Sessions"), QStringLiteral("X11DisplayOffset"),
                     T::Int, QStringLiteral("Sessions"), QStringLiteral("First X11 display number"),
                     QStringLiteral("Sessions use displays starting at this number."), QStringLiteral("10"));
        d.minimum = 1;
        d.maximum = 1000;
        l << d;
    }

    // ---- Desktop ---------------------------------------------------------------------------
    l << def(QStringLiteral("desktop.enableUserWindowManager"), S, QStringLiteral("Globals"),
             QStringLiteral("EnableUserWindowManager"), T::Bool, QStringLiteral("Desktop"),
             QStringLiteral("Allow per-user startwm.sh"),
             QStringLiteral("Users may override the desktop with ~/startwm.sh."), QStringLiteral("false"));
    {
        auto d = def(QStringLiteral("desktop.defaultWindowManager"), S, QStringLiteral("Globals"),
                     QStringLiteral("DefaultWindowManager"), T::Path, QStringLiteral("Desktop"),
                     QStringLiteral("Desktop start script"),
                     QStringLiteral("Script sesman runs to start the desktop. Set via the desktop selector."),
                     QStringLiteral("startwm.sh"));
        d.internalOnly = true; // ConfigureDesktop
        l << d;
    }

    // ---- Redirection -----------------------------------------------------------------------
    l << def(QStringLiteral("redirect.clipboard"), X, QStringLiteral("Channels"), QStringLiteral("cliprdr"), T::Bool,
             QStringLiteral("Redirection"), QStringLiteral("Clipboard channel"),
             QStringLiteral("Allow clipboard sharing between client and session."), QStringLiteral("true"));
    {
        auto d = def(QStringLiteral("redirect.clipboardOutbound"), S, QStringLiteral("Security"),
                     QStringLiteral("RestrictOutboundClipboard"), T::EnumList, QStringLiteral("Redirection"),
                     QStringLiteral("Block copy from session to client"),
                     QStringLiteral("none, all, or any of text,file,image."), QStringLiteral("none"));
        d.choices = {QStringLiteral("none"), QStringLiteral("all"), QStringLiteral("text"), QStringLiteral("file"),
                     QStringLiteral("image")};
        l << d;
    }
    {
        auto d = def(QStringLiteral("redirect.clipboardInbound"), S, QStringLiteral("Security"),
                     QStringLiteral("RestrictInboundClipboard"), T::EnumList, QStringLiteral("Redirection"),
                     QStringLiteral("Block copy from client to session"),
                     QStringLiteral("none, all, or any of text,file,image."), QStringLiteral("none"));
        d.choices = {QStringLiteral("none"), QStringLiteral("all"), QStringLiteral("text"), QStringLiteral("file"),
                     QStringLiteral("image")};
        l << d;
    }
    l << def(QStringLiteral("redirect.drives"), X, QStringLiteral("Channels"), QStringLiteral("rdpdr"), T::Bool,
             QStringLiteral("Redirection"), QStringLiteral("Drive redirection"),
             QStringLiteral("Allow client drives to appear in the session (FUSE mount ~/thinclient_drives). "
                            "Printer redirection is not implemented by xrdp 0.9."), QStringLiteral("true"));
    l << def(QStringLiteral("redirect.audio"), X, QStringLiteral("Channels"), QStringLiteral("rdpsnd"), T::Bool,
             QStringLiteral("Redirection"), QStringLiteral("Audio output"),
             QStringLiteral("Allow sound playback to the client. Requires pipewire-module-xrdp or pulseaudio-module-xrdp."),
             QStringLiteral("true"));
    l << def(QStringLiteral("redirect.dynamicChannels"), X, QStringLiteral("Channels"), QStringLiteral("drdynvc"), T::Bool,
             QStringLiteral("Redirection"), QStringLiteral("Dynamic virtual channels"),
             QStringLiteral("Needed by some extensions (e.g. display resize)."), QStringLiteral("true"));
    l << def(QStringLiteral("redirect.allChannels"), X, QStringLiteral("Globals"), QStringLiteral("allow_channels"),
             T::Bool, QStringLiteral("Redirection"), QStringLiteral("Allow virtual channels"),
             QStringLiteral("Master switch for every channel in [Channels]. false blocks clipboard, drives, audio, ..."),
             QStringLiteral("true"));
    l << def(QStringLiteral("redirect.remoteApps"), X, QStringLiteral("Channels"), QStringLiteral("rail"), T::Bool,
             QStringLiteral("Redirection"), QStringLiteral("RemoteApp (RAIL) channel"),
             QStringLiteral("Lets clients show single remote programs in their own windows (/app)."), QStringLiteral("true"));
    l << def(QStringLiteral("redirect.driveMount"), S, QStringLiteral("Chansrv"), QStringLiteral("EnableFuseMount"),
             T::Bool, QStringLiteral("Redirection"), QStringLiteral("Mount redirected drives"),
             QStringLiteral("When false, drive redirection is negotiated but no FUSE folder is mounted."),
             QStringLiteral("true"));
    {
        auto d = def(QStringLiteral("redirect.driveFolder"), S, QStringLiteral("Chansrv"), QStringLiteral("FuseMountName"),
                     T::String, QStringLiteral("Redirection"), QStringLiteral("Drive folder name"),
                     QStringLiteral("Relative to the home directory, or absolute; %u = uid, %U = user name."),
                     QStringLiteral("xrdp_client"));
        l << d;
    }
    l << def(QStringLiteral("display.multiMonitor"), X, QStringLiteral("Globals"), QStringLiteral("allow_multimon"),
             T::Bool, QStringLiteral("Display"), QStringLiteral("Multiple monitors"),
             QStringLiteral("Allow clients to span the session over several monitors (/multimon)."), QStringLiteral("true"));
    l << def(QStringLiteral("display.bulkCompression"), X, QStringLiteral("Globals"), QStringLiteral("bulk_compression"),
             T::Bool, QStringLiteral("Display"), QStringLiteral("Bulk compression"),
             QStringLiteral("Compress RDP data. Saves bandwidth at a small CPU cost."), QStringLiteral("true"));
    {
        auto d = def(QStringLiteral("display.fastPath"), X, QStringLiteral("Globals"), QStringLiteral("use_fastpath"),
                     T::Enum, QStringLiteral("Display"), QStringLiteral("Fast-path"),
                     QStringLiteral("Lower-overhead encoding for input and screen updates."), QStringLiteral("both"));
        d.choices = {QStringLiteral("both"), QStringLiteral("input"), QStringLiteral("output"), QStringLiteral("none")};
        l << d;
    }
    {
        auto d = def(QStringLiteral("display.maxBpp"), X, QStringLiteral("Globals"), QStringLiteral("max_bpp"), T::Enum,
                     QStringLiteral("Display"), QStringLiteral("Maximum colour depth"),
                     QStringLiteral("Upper bound on bits per pixel. Clients choose resolution themselves."),
                     QStringLiteral("32"));
        d.choices = {QStringLiteral("8"), QStringLiteral("15"), QStringLiteral("16"), QStringLiteral("24"), QStringLiteral("32")};
        l << d;
    }

    // ---- Logging ---------------------------------------------------------------------------
    {
        auto d = def(QStringLiteral("logging.xrdpLevel"), X, QStringLiteral("Logging"), QStringLiteral("LogLevel"), T::Enum,
                     QStringLiteral("Logging"), QStringLiteral("xrdp log level"), QStringLiteral("Verbosity of xrdp."),
                     QStringLiteral("INFO"));
        d.choices = logLevels;
        l << d;
    }
    {
        auto d = def(QStringLiteral("logging.sesmanLevel"), S, QStringLiteral("Logging"), QStringLiteral("LogLevel"), T::Enum,
                     QStringLiteral("Logging"), QStringLiteral("Session manager log level"),
                     QStringLiteral("Verbosity of xrdp-sesman."), QStringLiteral("INFO"));
        d.choices = logLevels;
        l << d;
    }
    return l;
}

} // namespace

// Plain-language wording for people who are not system administrators. The technical
// file/key names remain visible in small print in the UI.
static void applyPlainLanguage(QList<SettingDef> &list)
{
    struct Text { const char *id; const char *label; const char *help; QList<const char *> choiceLabels; };
    const QList<Text> texts{
        {"network.listen", "Network port",
         "The number other computers use to reach this one. Keep 3389 unless your IT person tells you otherwise. "
         "You can also enter an address to accept connections on one network only (for example tcp://192.168.1.10:3389). "
         "Only allow access from your office network or a VPN, never directly from the internet.", {}},
        {"network.tcpKeepalive", "Notice dropped connections",
         "Detects when a remote computer disappears without signing off (for example, Wi-Fi drops), so its desktop is marked as \"away\".", {}},
        {"security.layer", "Connection security",
         "\"Encrypted only\" is the safe choice. \"Encrypted when possible\" also lets very old programs connect with weaker protection.",
         {"Encrypted only (recommended)", "Encrypted when possible", "Old insecure mode (not allowed)"}},
        {"security.cryptLevel", "Encryption strength for very old programs",
         "Only used when an old remote desktop program cannot use modern encryption. Keep \"High\".",
         {"Low", "Medium", "High (recommended)", "FIPS"}},
        {"security.tlsProtocols", "Allowed encryption versions",
         "Which versions of modern encryption are accepted. Keep both ticked.", {}},
        {"security.tlsCiphers", "Encryption details (advanced)",
         "Leave empty unless your security team gives you a value.", {}},
        {"security.allowRootLogin", "Allow the \"root\" administrator account to connect",
         "Strongly recommended: Off. Nobody needs to sign in remotely as root.", {}},
        {"security.maxLoginRetry", "Password attempts per connection",
         "How many wrong passwords someone may type before the connection is closed. 0 means no limit.", {}},
        {"sessions.maxSessions", "Maximum number of people", "How many remote desktops may be open at the same time.", {}},
        {"sessions.policy", "When someone reconnects",
         "Decides whether a person who connects again returns to the desktop they left open, or gets a new one.",
         {"Return to their open desktop (recommended)", "Return only if the screen size is the same",
          "Return only when connecting from the same computer", "Always open a new desktop",
          "Return only with the same screen size and computer", "New desktop for every connection (by screen size)"}},
        {"sessions.killDisconnected", "Close desktops that were left open",
         "If someone disconnects without signing out, close their desktop after the time below.", {}},
        {"sessions.disconnectedTimeLimit", "Close left-open desktops after (seconds)", "0 means immediately.", {}},
        {"sessions.idleTimeLimit", "Disconnect inactive people after (seconds)",
         "Disconnects people who have not used the mouse or keyboard for this long. Their desktop stays open. 0 means never.", {}},
        {"sessions.displayOffset", "First screen number (advanced)", "Internal numbering of remote screens. Keep 10.", {}},
        {"desktop.enableUserWindowManager", "Let people choose their own desktop program (advanced)",
         "Allows each user to start a different desktop with a file named startwm.sh in their home folder.", {}},
        {"redirect.clipboard", "Copy and paste between computers",
         "Lets people copy text and files between their own computer and the remote desktop.", {}},
        {"redirect.clipboardOutbound", "Block copying FROM this computer",
         "Stops people from copying information out of the remote desktop to their own computer.",
         {"Allow everything", "Block everything", "Block text", "Block files", "Block pictures"}},
        {"redirect.clipboardInbound", "Block copying TO this computer",
         "Stops people from pasting information from their own computer into the remote desktop.",
         {"Allow everything", "Block everything", "Block text", "Block files", "Block pictures"}},
        {"redirect.drives", "Access to the user's own disks and USB sticks",
         "Shows the connecting computer's drives inside the remote desktop (in a folder called thinclient_drives). Printers cannot be shared.", {}},
        {"redirect.audio", "Sound and microphone",
         "Sends sound from the remote desktop to the person's speakers, and their microphone into the remote desktop. "
         "Needs the \"Sound\" part from Setup.", {}},
        {"redirect.dynamicChannels", "Extra features (microphone, screen resizing and more)",
         "Needed for the microphone, for resizing the remote window and for other modern features. Keep On.", {}},
        {"redirect.allChannels", "Allow sharing between computers at all",
         "Main switch for everything in this section. When Off, clipboard, disks, sound and microphone are all blocked. Keep On.", {}},
        {"redirect.remoteApps", "Single programs in their own window (RemoteApp)",
         "Lets the connecting computer show one remote program in a normal window instead of the whole desktop.", {}},
        {"redirect.driveMount", "Show the user's disks as a folder",
         "When On, the connecting computer's disks and USB sticks appear as a folder in the remote desktop.", {}},
        {"redirect.driveFolder", "Name of the folder with the user's disks",
         "Where the connecting computer's disks appear, inside each person's home folder. Keep \"thinclient_drives\".", {}},
        {"display.multiMonitor", "Use several monitors",
         "Lets people with two or more monitors spread the remote desktop over all of them.", {}},
        {"display.bulkCompression", "Compress data",
         "Uses less network bandwidth. Keep On.", {}},
        {"display.fastPath", "Faster mouse, keyboard and screen updates",
         "Keep \"Both\". Only change it if an old remote desktop program has problems.",
         {"Both (recommended)", "Only for mouse and keyboard", "Only for the screen", "Off"}},
        {"display.maxBpp", "Colour quality",
         "Lower quality uses less network bandwidth. The screen size is chosen on the connecting computer.",
         {"Very low (256 colours)", "Low", "Medium", "High", "Best (recommended)"}},
        {"logging.xrdpLevel", "How much detail to record (connections)",
         "\"Normal\" is right for everyday use. \"Everything\" helps when troubleshooting.",
         {"Minimal", "Errors only", "Warnings and errors", "Normal (recommended)", "Everything (troubleshooting)"}},
        {"logging.sesmanLevel", "How much detail to record (desktops)",
         "\"Normal\" is right for everyday use. \"Everything\" helps when troubleshooting.",
         {"Minimal", "Errors only", "Warnings and errors", "Normal (recommended)", "Everything (troubleshooting)"}},
    };
    for (SettingDef &d : list) {
        for (const Text &t : texts) {
            if (d.id != QLatin1String(t.id))
                continue;
            d.label = QString::fromUtf8(t.label);
            d.help = QString::fromUtf8(t.help);
            if (t.choiceLabels.size() == d.choices.size()) {
                d.choiceLabels.clear();
                for (const char *c : t.choiceLabels)
                    d.choiceLabels << QString::fromUtf8(c);
            }
        }
        if (d.choiceLabels.isEmpty())
            d.choiceLabels = d.choices;
    }
}

const QList<SettingDef> &ConfigSchema::settings()
{
    static const QList<SettingDef> s = [] {
        QList<SettingDef> l = buildSettings();
        applyPlainLanguage(l);
        return l;
    }();
    return s;
}

const SettingDef *ConfigSchema::find(const QString &id)
{
    for (const auto &d : settings())
        if (d.id == id)
            return &d;
    return nullptr;
}

const SettingDef *ConfigSchema::find(ConfigFile file, const QString &section, const QString &key)
{
    for (const auto &d : settings())
        if (d.file == file && d.section.compare(section, Qt::CaseInsensitive) == 0
            && d.key.compare(key, Qt::CaseInsensitive) == 0)
            return &d;
    return nullptr;
}

std::optional<QString> ConfigSchema::validate(const SettingDef &d, const QString &rawValue, const XrdpVersion &version)
{
    const QString value = rawValue.trimmed();
    if (!validators::isSafeIniValue(value))
        return QStringLiteral("Value contains forbidden characters");
    if (version.valid && version < d.minVersion)
        return QStringLiteral("%1 requires xrdp %2 or newer").arg(d.label, d.minVersion.toString());
    if (value.isEmpty())
        return d.allowEmpty ? std::nullopt : std::optional<QString>(QStringLiteral("%1 must not be empty").arg(d.label));

    switch (d.type) {
    case T::Bool: {
        static const QStringList ok{QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("yes"),
                                    QStringLiteral("no"), QStringLiteral("1"), QStringLiteral("0")};
        if (!ok.contains(value.toLower()))
            return QStringLiteral("%1 must be true or false").arg(d.label);
        return std::nullopt;
    }
    case T::Int: {
        bool ok = false;
        const int n = value.toInt(&ok);
        if (!ok || n < d.minimum || n > d.maximum)
            return QStringLiteral("%1 must be a whole number between %2 and %3").arg(d.label).arg(d.minimum).arg(d.maximum);
        return std::nullopt;
    }
    case T::Enum:
        for (const auto &c : d.choices)
            if (c.compare(value, Qt::CaseInsensitive) == 0) {
                if (d.id == QLatin1String("security.layer") && value.compare(QLatin1String("rdp"), Qt::CaseInsensitive) == 0)
                    return QStringLiteral("Standard RDP Security (security_layer=rdp) is not allowed: it has no server authentication");
                return std::nullopt;
            }
        return QStringLiteral("%1 must be one of: %2").arg(d.label, d.choices.join(QStringLiteral(", ")));
    case T::EnumList: {
        const QStringList parts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return QStringLiteral("%1 must not be empty").arg(d.label);
        for (QString p : parts) {
            p = p.trimmed().toLower();
            if (!d.choices.contains(p))
                return QStringLiteral("\"%1\" is not valid for %2").arg(p, d.label);
        }
        return std::nullopt;
    }
    case T::String:
        if (d.id == QLatin1String("redirect.driveFolder")) {
            static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_./%-]+$"));
            if (!re.match(value).hasMatch() || value.split(QLatin1Char('/')).contains(QStringLiteral("..")))
                return QStringLiteral("%1 may contain only letters, digits and _ . / % - (no \"..\")").arg(d.label);
        }
        return std::nullopt;
    case T::Path:
        if (d.id == QLatin1String("desktop.defaultWindowManager") && value == QLatin1String("startwm.sh"))
            return std::nullopt;
        if (!validators::isSafeAbsolutePath(value))
            return QStringLiteral("%1 must be an absolute, normalised path").arg(d.label);
        return std::nullopt;
    case T::ListenSpec: {
        QString err;
        if (!ListenSpec::parse(value, &err))
            return err;
        return std::nullopt;
    }
    case T::GroupName:
        if (!validators::isValidGroupName(value))
            return QStringLiteral("\"%1\" is not a valid group name").arg(value);
        return std::nullopt;
    case T::CipherList:
        if (!validators::isValidCipherList(value))
            return QStringLiteral("Cipher list contains invalid characters");
        return std::nullopt;
    case T::TlsProtocols: {
        const QStringList parts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return QStringLiteral("At least one TLS protocol is required");
        for (const QString &p : parts) {
            const QString t = p.trimmed();
            if (t != QLatin1String("TLSv1.2") && t != QLatin1String("TLSv1.3"))
                return QStringLiteral("Protocol \"%1\" is not allowed; use TLSv1.2 and/or TLSv1.3").arg(t);
        }
        return std::nullopt;
    }
    case T::MaxSessions: {
        bool ok = false;
        const int n = value.toInt(&ok);
        if (!ok || n < 0)
            return QStringLiteral("Maximum sessions must be a non-negative whole number");
        const SessionLimitPolicy policy(version);
        if (n == 0 && !policy.zeroMeansUnlimited())
            return QStringLiteral("MaxSessions=0 would deny every login on xrdp %1; use the Unlimited option instead")
                .arg(version.toString());
        if (n > SessionLimitPolicy::kLegacyUnlimitedSentinel)
            return QStringLiteral("Maximum sessions must not exceed %1").arg(SessionLimitPolicy::kLegacyUnlimitedSentinel);
        return std::nullopt;
    }
    }
    return QStringLiteral("Unsupported setting type");
}

} // namespace urdm
