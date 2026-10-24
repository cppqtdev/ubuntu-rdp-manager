#include "RdpNegotiation.h"

#include <QTest>

using namespace urdm::rdp;

class TstRdpNegotiation : public QObject
{
    Q_OBJECT

private slots:
    void requestLayout()
    {
        const QByteArray req = buildConnectionRequest(ProtocolSsl | ProtocolHybrid, "u");
        // TPKT
        QCOMPARE(uchar(req[0]), uchar(0x03));
        QCOMPARE(int((uchar(req[2]) << 8) | uchar(req[3])), int(req.size()));
        // X.224 CR, LI = length of the rest of the TPDU
        QCOMPARE(uchar(req[5]), uchar(0xE0));
        QCOMPARE(int(uchar(req[4])), int(req.size() - 5));
        QVERIFY(req.contains("Cookie: mstshash=u\r\n"));
        // RDP_NEG_REQ trailer
        const QByteArray neg = req.right(8);
        QCOMPARE(neg, QByteArray::fromHex("0100080003000000"));
    }

    void parsesSelectedTls()
    {
        // X.224 CC + RDP_NEG_RSP selecting PROTOCOL_SSL
        QByteArray r = QByteArray::fromHex("03000013" "0e" "d0" "0000" "1234" "00" "02" "00" "0800" "01000000");
        const auto res = parseConnectionConfirm(r);
        QCOMPARE(res.kind, NegotiationResult::Kind::Selected);
        QCOMPARE(res.selectedProtocol, quint32(ProtocolSsl));
        QCOMPARE(res.consumedBytes, 19);
    }

    void parsesFallbackToStandardRdp()
    {
        // TLS unusable (e.g. unreadable key): server selects PROTOCOL_RDP
        QByteArray r = QByteArray::fromHex("03000013" "0e" "d0" "0000" "1234" "00" "02" "00" "0800" "00000000");
        const auto res = parseConnectionConfirm(r);
        QCOMPARE(res.kind, NegotiationResult::Kind::Selected);
        QCOMPARE(res.selectedProtocol, quint32(ProtocolRdp));
    }

    void parsesFailure()
    {
        QByteArray r = QByteArray::fromHex("03000013" "0e" "d0" "0000" "1234" "00" "03" "00" "0800" "05000000");
        const auto res = parseConnectionConfirm(r);
        QCOMPARE(res.kind, NegotiationResult::Kind::Failure);
        QCOMPARE(res.failureCode, quint32(5));
        QVERIFY(failureText(5).contains(QStringLiteral("NLA")));
    }

    void incompleteAndMalformed()
    {
        QCOMPARE(parseConnectionConfirm(QByteArray::fromHex("0300")).kind, NegotiationResult::Kind::Incomplete);
        QCOMPARE(parseConnectionConfirm(QByteArray::fromHex("03000013" "0e" "d0")).kind, NegotiationResult::Kind::Incomplete);
        QCOMPARE(parseConnectionConfirm(QByteArray::fromHex("16030100")).kind, NegotiationResult::Kind::Malformed); // TLS, not TPKT
        QCOMPARE(parseConnectionConfirm(QByteArray::fromHex("0300000b" "06" "d0" "0000" "1234" "00")).kind,
                 NegotiationResult::Kind::NoNegotiationData);
    }
};

QTEST_GUILESS_MAIN(TstRdpNegotiation)
#include "tst_rdpnegotiation.moc"
