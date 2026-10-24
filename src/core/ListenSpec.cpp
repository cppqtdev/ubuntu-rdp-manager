#include "ListenSpec.h"
#include "Validators.h"

#include <QHostAddress>
#include <QRegularExpression>

namespace urdm {

bool ListenEndpoint::isLoopbackOnly() const
{
    if (!isTcp())
        return scheme == Scheme::Unix;
    if (address == QLatin1String("."))
        return true;
    const QHostAddress a(address);
    return !a.isNull() && a.isLoopback();
}

bool ListenEndpoint::isAllInterfaces() const
{
    if (!isTcp())
        return false;
    if (address.isEmpty())
        return true;
    const QHostAddress a(address);
    return a == QHostAddress::AnyIPv4 || a == QHostAddress::AnyIPv6;
}

QString ListenEndpoint::toString() const
{
    switch (scheme) {
    case Scheme::Bare:
        return QString::number(port);
    case Scheme::Tcp:
        return QStringLiteral("tcp://%1:%2").arg(address).arg(port);
    case Scheme::Tcp6:
        if (address.isEmpty() || address == QLatin1String("."))
            return QStringLiteral("tcp6://%1:%2").arg(address).arg(port);
        return QStringLiteral("tcp6://{%1}:%2").arg(address).arg(port);
    case Scheme::Unix:
        return QStringLiteral("unix://%1").arg(path);
    case Scheme::Vsock:
        return QStringLiteral("vsock://%1:%2").arg(vsockCid).arg(port);
    }
    return {};
}

QString ListenEndpoint::displayAddress() const
{
    switch (scheme) {
    case Scheme::Bare:
        return QStringLiteral("all interfaces:%1").arg(port);
    case Scheme::Tcp:
    case Scheme::Tcp6: {
        QString host = address.isEmpty() ? QStringLiteral("all interfaces")
                       : address == QLatin1String(".") ? (scheme == Scheme::Tcp ? QStringLiteral("127.0.0.1")
                                                                                : QStringLiteral("::1"))
                                                       : address;
        return QStringLiteral("%1:%2").arg(host).arg(port);
    }
    case Scheme::Unix:
        return QStringLiteral("unix socket %1").arg(path);
    case Scheme::Vsock:
        return QStringLiteral("vsock %1:%2").arg(vsockCid).arg(port);
    }
    return {};
}

std::optional<ListenSpec> ListenSpec::parse(const QString &value, QString *error)
{
    auto fail = [error](const QString &msg) -> std::optional<ListenSpec> {
        if (error)
            *error = msg;
        return std::nullopt;
    };

    static const QRegularExpression sep(QStringLiteral(R"([\s,]+)"));
    const QStringList tokens = value.trimmed().split(sep, Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return fail(QStringLiteral("At least one listening port is required"));

    static const QRegularExpression bareRe(QStringLiteral(R"(^(\d{1,5})$)"));
    static const QRegularExpression tcpRe(QStringLiteral(R"(^tcp://([^:/{}]*):(\d{1,5})$)"));
    static const QRegularExpression tcp6Re(QStringLiteral(R"(^tcp6://(\{([0-9A-Fa-f:.]+)\}|\.|):(\d{1,5})$)"));
    static const QRegularExpression unixRe(QStringLiteral(R"(^unix://(.+)$)"));
    static const QRegularExpression vsockRe(QStringLiteral(R"(^vsock://(-?\d+):(\d{1,5})$)"));

    ListenSpec spec;
    for (const QString &t : tokens) {
        ListenEndpoint ep;
        if (auto m = bareRe.match(t); m.hasMatch()) {
            ep.scheme = ListenEndpoint::Scheme::Bare;
            ep.port = m.captured(1).toInt();
        } else if (auto m2 = tcpRe.match(t); m2.hasMatch()) {
            ep.scheme = ListenEndpoint::Scheme::Tcp;
            ep.address = m2.captured(1);
            ep.port = m2.captured(2).toInt();
            if (!ep.address.isEmpty() && ep.address != QLatin1String(".")) {
                QHostAddress a;
                if (!a.setAddress(ep.address) || a.protocol() != QAbstractSocket::IPv4Protocol)
                    return fail(QStringLiteral("\"%1\" is not a valid IPv4 address").arg(ep.address));
            }
        } else if (auto m3 = tcp6Re.match(t); m3.hasMatch()) {
            ep.scheme = ListenEndpoint::Scheme::Tcp6;
            ep.address = m3.captured(2).isEmpty() ? m3.captured(1) : m3.captured(2);
            ep.port = m3.captured(3).toInt();
            if (!ep.address.isEmpty() && ep.address != QLatin1String(".")) {
                QHostAddress a;
                if (!a.setAddress(ep.address) || a.protocol() != QAbstractSocket::IPv6Protocol)
                    return fail(QStringLiteral("\"%1\" is not a valid IPv6 address").arg(ep.address));
            }
        } else if (auto m4 = unixRe.match(t); m4.hasMatch()) {
            ep.scheme = ListenEndpoint::Scheme::Unix;
            ep.path = m4.captured(1);
            if (ep.path.contains(QLatin1String("..")) || !validators::isSafeIniValue(ep.path))
                return fail(QStringLiteral("Invalid unix socket path \"%1\"").arg(ep.path));
        } else if (auto m5 = vsockRe.match(t); m5.hasMatch()) {
            ep.scheme = ListenEndpoint::Scheme::Vsock;
            ep.vsockCid = m5.captured(1);
            ep.port = m5.captured(2).toInt();
        } else {
            return fail(QStringLiteral("Unrecognised listen specification \"%1\"").arg(t));
        }
        if (ep.scheme != ListenEndpoint::Scheme::Unix && !validators::isValidTcpPort(ep.port))
            return fail(QStringLiteral("Port %1 is outside 1-65535").arg(ep.port));
        spec.m_endpoints.append(ep);
    }
    return spec;
}

ListenSpec ListenSpec::singleTcp(const QString &address, int port)
{
    ListenSpec spec;
    ListenEndpoint ep;
    port = qBound(1, port, 65535);
    if (address.isEmpty()) {
        ep.scheme = ListenEndpoint::Scheme::Bare;
    } else {
        const QHostAddress a(address);
        ep.scheme = (a.protocol() == QAbstractSocket::IPv6Protocol) ? ListenEndpoint::Scheme::Tcp6
                                                                     : ListenEndpoint::Scheme::Tcp;
        ep.address = address;
    }
    ep.port = port;
    spec.m_endpoints.append(ep);
    return spec;
}

QList<int> ListenSpec::tcpPorts() const
{
    QList<int> ports;
    for (const auto &ep : m_endpoints)
        if (ep.isTcp() && !ports.contains(ep.port))
            ports << ep.port;
    return ports;
}

bool ListenSpec::exposesNonLoopback() const
{
    for (const auto &ep : m_endpoints)
        if (ep.scheme != ListenEndpoint::Scheme::Unix && !ep.isLoopbackOnly())
            return true;
    return false;
}

QString ListenSpec::toString() const
{
    QStringList parts;
    for (const auto &ep : m_endpoints)
        parts << ep.toString();
    return parts.join(QLatin1Char(' '));
}

} // namespace urdm
