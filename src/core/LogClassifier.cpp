#include "LogClassifier.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace urdm {

QStringList LogClassifier::categories()
{
    return {QStringLiteral("auth-failure"), QStringLiteral("access-denied"), QStringLiteral("limit-denied"),
            QStringLiteral("session-start"), QStringLiteral("session-end"), QStringLiteral("session-failure"),
            QStringLiteral("connection"), QStringLiteral("tls-problem"), QStringLiteral("service"),
            QStringLiteral("error"), QStringLiteral("info")};
}

std::optional<LogEntry> LogClassifier::parseJournalJson(const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject o = doc.object();
    LogEntry e;
    e.timeMs = o.value(QStringLiteral("__REALTIME_TIMESTAMP")).toString().toLongLong() / 1000;
    e.priority = o.value(QStringLiteral("PRIORITY")).toString(QStringLiteral("6")).toInt();
    e.source = o.value(QStringLiteral("SYSLOG_IDENTIFIER")).toString();
    if (e.source.isEmpty())
        e.source = o.value(QStringLiteral("_SYSTEMD_UNIT")).toString();
    e.cursor = o.value(QStringLiteral("__CURSOR")).toString();
    const QJsonValue msg = o.value(QStringLiteral("MESSAGE"));
    if (msg.isArray()) { // journald encodes non-UTF-8 messages as byte arrays
        QByteArray bytes;
        for (const auto &b : msg.toArray())
            bytes.append(char(b.toInt()));
        e.message = QString::fromUtf8(bytes);
    } else {
        e.message = msg.toString();
    }
    classify(e);
    return e;
}

void LogClassifier::classify(LogEntry &e)
{
    struct Rule {
        QRegularExpression re;
        const char *category;
        int userGroup;
        int addressGroup;
        bool primary = true; // the one line per event that counters use
    };
    static const QList<Rule> rules{
        {QRegularExpression(QStringLiteral(R"(AUTHFAIL: user=(\S+) ip=(\S+))")), "auth-failure", 1, 2},
        {QRegularExpression(QStringLiteral(R"(pam_authenticate failed)")), "auth-failure", 0, 0, false},
        {QRegularExpression(QStringLiteral(R"(authentication failure;.*\buser=(\S+))")), "auth-failure", 1, 0, false},
        {QRegularExpression(QStringLiteral(R"(login failed for user (\S+))")), "auth-failure", 1, 0, false},
        {QRegularExpression(QStringLiteral(R"(max concurrent session limit exceeded\. login for user (\S+) denied)")), "limit-denied", 1, 0},
        {QRegularExpression(QStringLiteral(R"(login denied for user (\S+))")), "access-denied", 1, 0},
        {QRegularExpression(QStringLiteral(R"(ROOT login attempted, but root login is disabled)")), "access-denied", 0, 0},
        {QRegularExpression(QStringLiteral(R"(\+\+ created session.*username (\S+), ip (\S+))")), "session-start", 1, 2},
        {QRegularExpression(QStringLiteral(R"(\+\+ (?:reconnected|terminated) session:?\s+username (\S+))")), "session-end", 1, 0},
        {QRegularExpression(QStringLiteral(R"(no display in range|X server.*(?:fail|did not start)|error starting|cannot start|startwm.*fail|Window manager .* exited)"),
                            QRegularExpression::CaseInsensitiveOption), "session-failure", 0, 0},
        {QRegularExpression(QStringLiteral(R"(Non-TLS connection established from (\S+))")), "tls-problem", 0, 1},
        {QRegularExpression(QStringLiteral(R"(Cannot read private key|Cannot accept TLS connections)"),
                            QRegularExpression::CaseInsensitiveOption), "tls-problem", 0, 0},
        // A client giving up during the TLS handshake (cancelled, untrusted certificate) is not a server fault.
        {QRegularExpression(QStringLiteral(R"(SSL_accept|SSL_shutdown|TLS (?:handshake|connection) failed)"),
                            QRegularExpression::CaseInsensitiveOption), "connection", 0, 0, false},
        {QRegularExpression(QStringLiteral(R"(connection received from (\S+) port (\d+))")), "connection", 0, 1},
        {QRegularExpression(QStringLiteral(R"(login successful for user (\S+))")), "connection", 1, 0, false},
    };
    for (const Rule &r : rules) {
        const auto m = r.re.match(e.message);
        if (!m.hasMatch())
            continue;
        e.category = QString::fromLatin1(r.category);
        e.primary = r.primary;
        if (r.userGroup)
            e.user = m.captured(r.userGroup).remove(QLatin1Char(','));
        if (r.addressGroup)
            e.address = m.captured(r.addressGroup).remove(QLatin1Char(','));
        e.plain = plainSentence(e);
        return;
    }
    if (e.source == QLatin1String("systemd")) {
        e.category = QStringLiteral("service");
        // keep "Started …"/"Stopped …" as the events; "Starting …", PID-file notes etc. are noise
        // only the main xrdp.service line (sesman is started/stopped together with it)
        e.primary = (e.message.startsWith(QLatin1String("Started ")) || e.message.startsWith(QLatin1String("Stopped ")))
                    && !e.message.contains(QLatin1String("sesman"));
    }
    else if (e.priority <= 3)
        e.category = QStringLiteral("error"), e.primary = false; // follow-up errors of an event
    else
        e.category = QStringLiteral("info"), e.primary = false;
    e.plain = plainSentence(e);
}

QString LogClassifier::plainSentence(const LogEntry &e)
{
    // Strip "::ffff:" from IPv4-mapped addresses and the trailing port for readability.
    QString from = e.address;
    from.remove(QStringLiteral("::ffff:"));
    static const QRegularExpression port(QStringLiteral(R"(:\d+$)"));
    if (from.count(QLatin1Char(':')) == 1)
        from.remove(port);
    const QString at = from.isEmpty() ? QString() : QStringLiteral(" (from %1)").arg(from);
    const QString who = e.user.isEmpty() ? QStringLiteral("someone") : QStringLiteral("\"%1\"").arg(e.user);
    if (e.category == QLatin1String("auth-failure"))
        return QStringLiteral("Wrong user name or password for %1%2").arg(who, at);
    if (e.category == QLatin1String("access-denied"))
        return e.user.isEmpty() ? QStringLiteral("Someone tried to connect who is not allowed")
                                : QStringLiteral("%1 tried to connect but is not on the allowed list").arg(who);
    if (e.category == QLatin1String("limit-denied"))
        return QStringLiteral("%1 was turned away because the connection limit was reached").arg(who);
    if (e.category == QLatin1String("session-start"))
        return QStringLiteral("%1 signed in and got a desktop%2").arg(who, at);
    if (e.category == QLatin1String("session-end"))
        return QStringLiteral("The desktop of %1 was closed").arg(who);
    if (e.category == QLatin1String("session-failure"))
        return QStringLiteral("A desktop could not be started after signing in");
    if (e.category == QLatin1String("tls-problem")) {
        if (e.message.contains(QLatin1String("Non-TLS")))
            return QStringLiteral("A connection was made WITHOUT encryption%1").arg(at);
        if (e.message.contains(QLatin1String("Cannot read private key")) || e.message.contains(QLatin1String("Cannot accept TLS")))
            return QStringLiteral("The server cannot use its security key, so encryption is not working");
        return QStringLiteral("A connection attempt stopped while setting up encryption (the other computer may have cancelled)");
    }
    if (e.category == QLatin1String("connection") && (e.message.contains(QLatin1String("SSL_")) || e.message.contains(QLatin1String("TLS "))))
        return QStringLiteral("A connection attempt stopped while setting up encryption (the other computer may have cancelled or not trusted the certificate)");
    if (e.category == QLatin1String("connection"))
        return e.user.isEmpty() ? QStringLiteral("A computer connected%1").arg(at) : QStringLiteral("%1 entered the correct password").arg(who);
    if (e.category == QLatin1String("service")) {
        if (e.message.startsWith(QLatin1String("Started")))
            return QStringLiteral("Remote Desktop was turned on");
        if (e.message.startsWith(QLatin1String("Stopped")))
            return QStringLiteral("Remote Desktop was turned off");
    }
    QString m = e.message;
    static const QRegularExpression level(QStringLiteral(R"(^\[(INFO|ERROR|WARN|DEBUG|CORE) *\] *)"));
    m.remove(level);
    return m;
}

} // namespace urdm
