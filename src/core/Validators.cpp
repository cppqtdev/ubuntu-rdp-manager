#include "Validators.h"

#include <QDir>
#include <QHostAddress>
#include <QRegularExpression>

namespace urdm::validators {

bool isValidUserName(const QString &name)
{
    static const QRegularExpression re(QStringLiteral(R"(^[a-z_][a-z0-9_-]{0,30}\$?$)"));
    return re.match(name).hasMatch();
}

bool isValidGroupName(const QString &name)
{
    return isValidUserName(name);
}

bool isValidLogindSessionId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral(R"(^[A-Za-z0-9]{1,32}$)"));
    return re.match(id).hasMatch();
}

std::optional<int> parseDisplayNumber(const QString &display)
{
    static const QRegularExpression re(QStringLiteral(R"(^:(\d{1,5})(?:\.\d{1,3})?$)"));
    const auto m = re.match(display.trimmed());
    if (!m.hasMatch())
        return std::nullopt;
    bool ok = false;
    const int n = m.captured(1).toInt(&ok);
    if (!ok || n < 0 || n > 65535)
        return std::nullopt;
    return n;
}

bool isSafeAbsolutePath(const QString &path)
{
    if (path.isEmpty() || path.size() > 4096 || !path.startsWith(QLatin1Char('/')))
        return false;
    for (const QChar c : path) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7f)
            return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        if (p == QLatin1String("..") || p == QLatin1String("."))
            return false;
    }
    return QDir::cleanPath(path) == path;
}

bool isValidTcpPort(int port)
{
    return port >= 1 && port <= 65535;
}

bool isSafeIniValue(const QString &value)
{
    if (value.size() > 1024)
        return false;
    for (const QChar c : value) {
        if (c == QLatin1Char('\n') || c == QLatin1Char('\r') || c.unicode() == 0)
            return false;
    }
    return true;
}

bool isValidCipherList(const QString &value)
{
    static const QRegularExpression re(QStringLiteral(R"(^[A-Za-z0-9:+!@_.,=-]{1,512}$)"));
    return re.match(value).hasMatch();
}

bool isValidHostName(const QString &host)
{
    if (host.isEmpty() || host.size() > 253)
        return false;
    QHostAddress addr;
    if (addr.setAddress(host))
        return true;
    static const QRegularExpression re(
        QStringLiteral(R"(^(?=.{1,253}$)([A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)(\.[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*\.?$)"));
    return re.match(host).hasMatch();
}

} // namespace urdm::validators
