#include "LogSanitizer.h"

#include <QRegularExpression>

namespace urdm {

QString LogSanitizer::sanitize(const QString &text)
{
    static const QString redacted = QStringLiteral("[REDACTED]");
    QString out = text;

    static const QRegularExpression pemKey(
        QStringLiteral(R"(-----BEGIN ([A-Z ]*)PRIVATE KEY-----.*?-----END \1PRIVATE KEY-----)"),
        QRegularExpression::DotMatchesEverythingOption);
    out.replace(pemKey, QStringLiteral("-----BEGIN PRIVATE KEY-----[REDACTED]-----END PRIVATE KEY-----"));

    // key=value / key: value pairs whose key looks secret
    static const QRegularExpression kv(
        QStringLiteral(R"(\b((?:pass(?:word|wd)?|pwd|secret|token|auth[_-]?token|api[_-]?key|credentials?)\s*[=:]\s*)(\"[^\"]*\"|'[^']*'|\S+))"),
        QRegularExpression::CaseInsensitiveOption);
    out.replace(kv, QStringLiteral("\\1") + redacted);

    // FreeRDP / generic CLI password arguments: /p:secret, -p secret, --password=secret
    static const QRegularExpression cliP(QStringLiteral(R"((\s/p:)\S+)"));
    out.replace(cliP, QStringLiteral("\\1") + redacted);
    static const QRegularExpression dashP(QStringLiteral(R"((\s-p\s+)\S+)"));
    out.replace(dashP, QStringLiteral("\\1") + redacted);

    // xrdp.ini allows {base64}<value> credentials
    static const QRegularExpression b64(QStringLiteral(R"(\{base64\}[A-Za-z0-9+/=]+)"));
    out.replace(b64, QStringLiteral("{base64}") + redacted);

    static const QRegularExpression bearer(QStringLiteral(R"((Bearer\s+)[A-Za-z0-9._~+/=-]+)"),
                                           QRegularExpression::CaseInsensitiveOption);
    out.replace(bearer, QStringLiteral("\\1") + redacted);
    return out;
}

} // namespace urdm
