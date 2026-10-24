#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace urdm::rdp {

// [MS-RDPBCGR] 2.2.1.1 / 2.2.1.2: X.224 Connection Request / Confirm carrying
// RDP_NEG_REQ / RDP_NEG_RSP / RDP_NEG_FAILURE inside a TPKT.
enum Protocol : quint32 {
    ProtocolRdp = 0x0,      // Standard RDP Security (no TLS)
    ProtocolSsl = 0x1,      // TLS
    ProtocolHybrid = 0x2,   // CredSSP (NLA)
    ProtocolHybridEx = 0x8,
};

QByteArray buildConnectionRequest(quint32 requestedProtocols, const QByteArray &cookieUser = QByteArrayLiteral("urdm-test"));

struct NegotiationResult
{
    enum class Kind { Incomplete, Selected, Failure, NoNegotiationData, Malformed };
    Kind kind = Kind::Incomplete;
    quint32 selectedProtocol = 0;
    quint32 failureCode = 0;
    int consumedBytes = 0;
};

// Parses a server reply (TPKT + X.224 Connection Confirm). Returns Incomplete until enough bytes arrived.
NegotiationResult parseConnectionConfirm(const QByteArray &data);

QString protocolName(quint32 protocol);
QString failureText(quint32 code);

} // namespace urdm::rdp
