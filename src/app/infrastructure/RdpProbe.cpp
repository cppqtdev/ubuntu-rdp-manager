#include "RdpProbe.h"

#include "RdpNegotiation.h"

#include <QSslCipher>
#include <QSslConfiguration>

namespace urdm::app {

RdpProbe::RdpProbe(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        const QString stage = !m_networkOk ? QStringLiteral("tcp") : !m_negotiated ? QStringLiteral("negotiation") : QStringLiteral("tls");
        fail(stage, stage == QLatin1String("tcp") ? QStringLiteral("Computer can be reached")
                    : stage == QLatin1String("negotiation") ? QStringLiteral("Remote desktop server answers")
                                                           : QStringLiteral("Connection is encrypted"),
             QStringLiteral("No answer (timed out)"));
    });
}

void RdpProbe::addStage(const QString &id, const QString &name, const QString &status, const QString &detail, qint64 ms)
{
    m_stages << QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("status"), status},
                            {QStringLiteral("detail"), detail}, {QStringLiteral("ms"), ms}};
}

void RdpProbe::start(const QString &host, int port, int timeoutMs)
{
    if (m_running)
        return;
    m_running = true;
    m_negotiated = m_networkOk = m_rdpOk = m_tlsOk = m_acceptedByPin = false;
    m_stages.clear();
    m_buffer.clear();
    m_sslErrors.clear();
    m_host = host;
    if (m_socket)
        m_socket->deleteLater();
    m_socket = new QSslSocket(this);
    m_clock.start();
    m_stageStart = 0;

    connect(m_socket, &QAbstractSocket::connected, this, [this, port]() {
        m_networkOk = true;
        addStage(QStringLiteral("tcp"), QStringLiteral("Computer can be reached"), QStringLiteral("pass"),
                 QStringLiteral("Reached %1 on port %2").arg(m_host).arg(port), m_clock.elapsed());
        m_stageStart = m_clock.elapsed();
        m_socket->write(rdp::buildConnectionRequest(rdp::ProtocolSsl | rdp::ProtocolHybrid));
    });
    connect(m_socket, &QAbstractSocket::readyRead, this, &RdpProbe::onReadyRead);
    connect(m_socket, &QSslSocket::encrypted, this, &RdpProbe::onEncrypted);
    connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        m_sslErrors = errors; // recorded; only an exact pinned-certificate match may proceed
        const QSslCertificate peer = m_socket->peerCertificate();
        if (!m_pinned.isEmpty() && !peer.isNull() && peer.digest(QCryptographicHash::Sha256) == m_pinned) {
            m_acceptedByPin = true;
            m_socket->ignoreSslErrors(errors);
        }
    });
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_running)
            return;
        if (!m_networkOk) {
            fail(QStringLiteral("tcp"), QStringLiteral("Computer can be reached"), m_socket->errorString());
        } else if (!m_negotiated) {
            fail(QStringLiteral("negotiation"), QStringLiteral("Remote desktop server answers"),
                 QStringLiteral("Connection closed before the server answered: %1").arg(m_socket->errorString()));
        } else {
            QStringList errs;
            for (const auto &e : std::as_const(m_sslErrors))
                errs << e.errorString();
            const QSslCertificate cert = m_socket->peerCertificate();
            QString detail = errs.isEmpty() ? m_socket->errorString() : errs.join(QStringLiteral("; "));
            if (!cert.isNull())
                detail += QStringLiteral(" — presented certificate: CN=%1, issuer %2, valid until %3")
                              .arg(cert.subjectInfo(QSslCertificate::CommonName).join(QLatin1Char(',')),
                                   cert.issuerInfo(QSslCertificate::CommonName).join(QLatin1Char(',')),
                                   cert.expiryDate().toString(Qt::ISODate));
            fail(QStringLiteral("tls"), errs.isEmpty() ? QStringLiteral("Connection is encrypted") : QStringLiteral("Computer identity confirmed"), detail);
        }
    });

    if (!QSslSocket::supportsSsl())
        addStage(QStringLiteral("tls-backend"), QStringLiteral("TLS support"), QStringLiteral("warn"),
                 QStringLiteral("Qt has no TLS backend available; the TLS stage cannot run"));
    m_timeout.start(timeoutMs);
    m_socket->connectToHost(host, quint16(port));
}

void RdpProbe::onReadyRead()
{
    if (m_negotiated)
        return;
    m_buffer += m_socket->readAll();
    const auto r = rdp::parseConnectionConfirm(m_buffer);
    using K = rdp::NegotiationResult::Kind;
    if (r.kind == K::Incomplete)
        return;
    m_negotiated = true;
    const qint64 ms = m_clock.elapsed() - m_stageStart;
    switch (r.kind) {
    case K::Selected:
        m_rdpOk = true;
        if (r.selectedProtocol == rdp::ProtocolRdp) {
            addStage(QStringLiteral("negotiation"), QStringLiteral("Remote desktop server answers"), QStringLiteral("warn"),
                     QStringLiteral("The server answered but did NOT turn on encryption. Usually it cannot read its security key. "
                                    "Fix: Settings → Security certificate → Create new certificate."), ms);
            addStage(QStringLiteral("tls"), QStringLiteral("Connection is encrypted"), QStringLiteral("fail"),
                     QStringLiteral("The server did not offer encryption"));
            complete();
            return;
        }
        addStage(QStringLiteral("negotiation"), QStringLiteral("Remote desktop server answers"), QStringLiteral("pass"),
                 QStringLiteral("The server answered and chose %1").arg(rdp::protocolName(r.selectedProtocol)), ms);
        if (!QSslSocket::supportsSsl()) {
            addStage(QStringLiteral("tls"), QStringLiteral("Connection is encrypted"), QStringLiteral("skipped"), QStringLiteral("No TLS backend"));
            complete();
            return;
        }
        m_stageStart = m_clock.elapsed();
        m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
        m_socket->setPeerVerifyName(m_host);
        m_socket->startClientEncryption();
        return;
    case K::Failure:
        m_rdpOk = true; // the server speaks RDP; it refused our protocol choice
        addStage(QStringLiteral("negotiation"), QStringLiteral("Remote desktop server answers"), QStringLiteral("fail"),
                 QStringLiteral("Server refused: %1").arg(rdp::failureText(r.failureCode)), ms);
        break;
    case K::NoNegotiationData:
        m_rdpOk = true;
        addStage(QStringLiteral("negotiation"), QStringLiteral("Remote desktop server answers"), QStringLiteral("warn"),
                 QStringLiteral("Server does not support security negotiation (legacy RDP only)"), ms);
        break;
    default:
        addStage(QStringLiteral("negotiation"), QStringLiteral("Remote desktop server answers"), QStringLiteral("fail"),
                 QStringLiteral("The service on this port did not answer like an RDP server"), ms);
        break;
    }
    complete();
}

void RdpProbe::onEncrypted()
{
    m_tlsOk = true;
    const qint64 ms = m_clock.elapsed() - m_stageStart;
    const QSslCipher cipher = m_socket->sessionCipher();
    addStage(QStringLiteral("tls"), QStringLiteral("Connection is encrypted"), QStringLiteral("pass"),
             QStringLiteral("%1, %2").arg(cipher.protocolString(), cipher.name()), ms);
    const QSslCertificate cert = m_socket->peerCertificate();
    const QString who = cert.subjectInfo(QSslCertificate::CommonName).join(QLatin1Char(','));
    if (m_acceptedByPin) {
        QStringList why;
        for (const auto &e : std::as_const(m_sslErrors))
            why << e.errorString();
        addStage(QStringLiteral("certificate"), QStringLiteral("Computer identity confirmed"), QStringLiteral("pass"),
                 QStringLiteral("This computer showed exactly its own certificate (%1). It was made on this computer, so other "
                                "computers will ask once whether to trust it — that is normal.")
                     .arg(who));
        Q_UNUSED(why)
    } else {
        addStage(QStringLiteral("certificate"), QStringLiteral("Computer identity confirmed"), QStringLiteral("pass"),
                 QStringLiteral("Trusted certificate for %1 (from %2), valid until %3")
                     .arg(who, cert.issuerInfo(QSslCertificate::CommonName).join(QLatin1Char(',')),
                          cert.expiryDate().toString(Qt::ISODate)));
    }
    complete();
}

void RdpProbe::fail(const QString &id, const QString &name, const QString &detail)
{
    addStage(id, name, QStringLiteral("fail"), detail, m_clock.elapsed() - m_stageStart);
    complete();
}

void RdpProbe::complete()
{
    if (!m_running)
        return;
    m_running = false;
    m_timeout.stop();
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
    }
    const QString manual = QStringLiteral("manual");
    addStage(QStringLiteral("auth"), QStringLiteral("Sign in"), manual,
             QStringLiteral("Needs a real user name and password — try it from another computer"));
    addStage(QStringLiteral("session"), QStringLiteral("Desktop opens"), manual,
             QStringLiteral("You will see the person on the \"Who's Connected\" page"));
    addStage(QStringLiteral("desktop"), QStringLiteral("Desktop appears"), manual,
             QStringLiteral("Check that the desktop is visible in the window"));
    emit finished(m_stages, m_networkOk, m_rdpOk, m_tlsOk);
}

} // namespace urdm::app
