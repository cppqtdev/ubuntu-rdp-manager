#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSslSocket>
#include <QTimer>
#include <QVariantList>

namespace urdm::app {

// Staged RDP connection test. A listening TCP port is NOT treated as success: the probe
// performs the X.224/RDP_NEG exchange and a verified TLS handshake. Certificate errors are
// never ignored, except when the presented certificate is byte-identical (SHA-256) to a pinned one. Authentication, session creation and desktop rendering need real credentials
// and a real client, so they are reported as manual steps.
class RdpProbe : public QObject
{
    Q_OBJECT
public:
    explicit RdpProbe(QObject *parent = nullptr);
    // When set, a certificate whose SHA-256 digest equals this value is accepted even if no CA
    // vouches for it (certificate pinning, as FreeRDP's /cert:fingerprint). Any other certificate
    // still fails verification.
    void setPinnedSha256(const QByteArray &digest) { m_pinned = digest; }
    void start(const QString &host, int port, int timeoutMs = 10000);
    bool running() const { return m_running; }

signals:
    void finished(const QVariantList &stages, bool networkOk, bool rdpOk, bool tlsOk);

private:
    void addStage(const QString &id, const QString &name, const QString &status, const QString &detail, qint64 ms = -1);
    void fail(const QString &id, const QString &name, const QString &detail);
    void complete();
    void onReadyRead();
    void onEncrypted();

    QPointer<QSslSocket> m_socket;
    QTimer m_timeout;
    QElapsedTimer m_clock;
    QByteArray m_buffer;
    QString m_host;
    QVariantList m_stages;
    QList<QSslError> m_sslErrors;
    QByteArray m_pinned;
    bool m_acceptedByPin = false;
    bool m_running = false;
    bool m_negotiated = false;
    bool m_networkOk = false, m_rdpOk = false, m_tlsOk = false;
    qint64 m_stageStart = 0;
};

} // namespace urdm::app
