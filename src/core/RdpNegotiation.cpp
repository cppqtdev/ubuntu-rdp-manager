#include "RdpNegotiation.h"

#include <QtEndian>

namespace urdm::rdp {

QByteArray buildConnectionRequest(quint32 requestedProtocols, const QByteArray &cookieUser)
{
    const QByteArray cookie = QByteArrayLiteral("Cookie: mstshash=") + cookieUser.left(64) + QByteArrayLiteral("\r\n");
    QByteArray x224;
    // LI is filled in below; CR CDT=0xE0, DST-REF=0, SRC-REF=0, class 0
    x224.append(char(0));
    x224.append(char(0xE0));
    x224.append(char(0)).append(char(0));
    x224.append(char(0)).append(char(0));
    x224.append(char(0));
    x224.append(cookie);
    // RDP_NEG_REQ
    x224.append(char(0x01));  // type
    x224.append(char(0x00));  // flags
    x224.append(char(0x08)).append(char(0x00)); // length (LE)
    char proto[4];
    qToLittleEndian(requestedProtocols, proto);
    x224.append(proto, 4);
    x224[0] = char(x224.size() - 1);

    QByteArray tpkt;
    tpkt.append(char(0x03)).append(char(0x00));
    char len[2];
    qToBigEndian(quint16(4 + x224.size()), len);
    tpkt.append(len, 2);
    return tpkt + x224;
}

NegotiationResult parseConnectionConfirm(const QByteArray &data)
{
    NegotiationResult r;
    if (data.size() < 4)
        return r;
    const auto *d = reinterpret_cast<const uchar *>(data.constData());
    if (d[0] != 0x03) {
        r.kind = NegotiationResult::Kind::Malformed;
        return r;
    }
    const int total = qFromBigEndian<quint16>(d + 2);
    if (total < 11 || total > 4096) {
        r.kind = NegotiationResult::Kind::Malformed;
        return r;
    }
    if (data.size() < total)
        return r;
    r.consumedBytes = total;
    const int li = d[4];
    if (d[5] != 0xD0 || li + 5 > total) { // CC TPDU
        r.kind = NegotiationResult::Kind::Malformed;
        return r;
    }
    const int negOffset = 4 + 7; // TPKT(4) + LI,CC,dst(2),src(2),class
    if (total < negOffset + 8) {
        r.kind = NegotiationResult::Kind::NoNegotiationData;
        return r;
    }
    const uchar type = d[negOffset];
    const quint32 value = qFromLittleEndian<quint32>(d + negOffset + 4);
    if (type == 0x02) {
        r.kind = NegotiationResult::Kind::Selected;
        r.selectedProtocol = value;
    } else if (type == 0x03) {
        r.kind = NegotiationResult::Kind::Failure;
        r.failureCode = value;
    } else {
        r.kind = NegotiationResult::Kind::Malformed;
    }
    return r;
}

QString protocolName(quint32 protocol)
{
    switch (protocol) {
    case ProtocolRdp: return QStringLiteral("old unencrypted-login mode (no TLS)");
    case ProtocolSsl: return QStringLiteral("encrypted mode (TLS)");
    case ProtocolHybrid: return QStringLiteral("CredSSP/NLA");
    case ProtocolHybridEx: return QStringLiteral("CredSSP/NLA (extended)");
    default: return QStringLiteral("unknown (0x%1)").arg(protocol, 0, 16);
    }
}

QString failureText(quint32 code)
{
    switch (code) {
    case 1: return QStringLiteral("server requires TLS (SSL_REQUIRED_BY_SERVER)");
    case 2: return QStringLiteral("server does not allow TLS (SSL_NOT_ALLOWED_BY_SERVER)");
    case 3: return QStringLiteral("server has no usable certificate (SSL_CERT_NOT_ON_SERVER)");
    case 4: return QStringLiteral("inconsistent flags");
    case 5: return QStringLiteral("server requires NLA (HYBRID_REQUIRED_BY_SERVER)");
    case 6: return QStringLiteral("server requires TLS with user authentication");
    default: return QStringLiteral("failure code %1").arg(code);
    }
}

} // namespace urdm::rdp
