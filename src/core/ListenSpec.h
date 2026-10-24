#pragma once

#include <QList>
#include <QString>
#include <optional>

namespace urdm {

// The xrdp.ini [Globals] "port" value. Grammar verified against xrdp 0.9.24's xrdp.ini:
//   3389 | tcp://.:3389 | tcp://:3389 | tcp://<ipv4>:3389 | tcp6://.:3389 | tcp6://:3389 |
//   tcp6://{<ipv6>}:3389 | unix://<path> | vsock://<cid>:<port>
// separated by spaces or commas. "." means loopback, an empty address means all interfaces.
struct ListenEndpoint
{
    enum class Scheme { Bare, Tcp, Tcp6, Unix, Vsock };
    Scheme scheme = Scheme::Bare;
    QString address;   // "", "." or an IP literal (without braces)
    int port = 0;
    QString path;      // unix only
    QString vsockCid;  // vsock only

    bool isTcp() const { return scheme == Scheme::Bare || scheme == Scheme::Tcp || scheme == Scheme::Tcp6; }
    bool isLoopbackOnly() const;
    bool isAllInterfaces() const;
    QString toString() const;
    QString displayAddress() const;
};

class ListenSpec
{
public:
    static std::optional<ListenSpec> parse(const QString &value, QString *error = nullptr);
    static ListenSpec singleTcp(const QString &address, int port);

    QList<ListenEndpoint> endpoints() const { return m_endpoints; }
    QList<int> tcpPorts() const;
    bool exposesNonLoopback() const;
    QString toString() const;

private:
    QList<ListenEndpoint> m_endpoints;
};

} // namespace urdm
