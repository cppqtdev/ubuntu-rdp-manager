#include "LogClassifier.h"

#include <QFile>
#include <QTest>

using namespace urdm;

class TstLogClassifier : public QObject
{
    Q_OBJECT

    static LogEntry entry(const QString &msg, int prio = 6, const QString &src = QStringLiteral("xrdp-sesman"))
    {
        LogEntry e;
        e.message = msg;
        e.priority = prio;
        e.source = src;
        LogClassifier::classify(e);
        return e;
    }

private slots:
    // exact messages captured from xrdp 0.9.24 on 2026-10-24
    void verifiedMessages()
    {
        auto e = entry(QStringLiteral("[INFO ] AUTHFAIL: user=rdpuser5 ip=::ffff:172.18.0.3 time=1792819030"));
        QCOMPARE(e.category, QStringLiteral("auth-failure"));
        QCOMPARE(e.user, QStringLiteral("rdpuser5"));
        QCOMPARE(e.address, QStringLiteral("::ffff:172.18.0.3"));

        e = entry(QStringLiteral("pam_unix(xrdp-sesman:auth): authentication failure; logname= uid=0 euid=0 tty=xrdp-sesman ruser= rhost=  user=rdpuser5"), 5);
        QCOMPARE(e.category, QStringLiteral("auth-failure"));
        QCOMPARE(e.user, QStringLiteral("rdpuser5"));

        e = entry(QStringLiteral("[INFO ] login denied for user rdpuser4"));
        QCOMPARE(e.category, QStringLiteral("access-denied"));
        QCOMPARE(e.user, QStringLiteral("rdpuser4"));

        e = entry(QStringLiteral("[ERROR] max concurrent session limit exceeded. login for user rdpuser1 denied"), 3);
        QCOMPARE(e.category, QStringLiteral("limit-denied"));
        QCOMPARE(e.user, QStringLiteral("rdpuser1"));

        e = entry(QStringLiteral("[INFO ] ++ created session (access granted): username rdpuser1, ip ::ffff:172.18.0.3:45832 - socket: 12"));
        QCOMPARE(e.category, QStringLiteral("session-start"));
        QCOMPARE(e.user, QStringLiteral("rdpuser1"));
        QCOMPARE(e.address, QStringLiteral("::ffff:172.18.0.3:45832"));

        e = entry(QStringLiteral("[INFO ] ++ terminated session:  username rdpuser1, display :10.0, session_pid 127, ip ::ffff:172.18.0.3:45832 - socket: 12"));
        QCOMPARE(e.category, QStringLiteral("session-end"));
        QCOMPARE(e.user, QStringLiteral("rdpuser1"));

        e = entry(QStringLiteral("[INFO ] Non-TLS connection established from ::ffff:172.18.0.3 port 44074: with security level : high"), 6, QStringLiteral("xrdp"));
        QCOMPARE(e.category, QStringLiteral("tls-problem"));
        e = entry(QStringLiteral("[ERROR] Cannot read private key file /etc/xrdp/key.pem: Permission denied"), 3, QStringLiteral("xrdp"));
        QCOMPARE(e.category, QStringLiteral("tls-problem"));

        e = entry(QStringLiteral("[INFO ] Socket 12: AF_INET6 connection received from ::ffff:172.18.0.3 port 46754"), 6, QStringLiteral("xrdp"));
        QCOMPARE(e.category, QStringLiteral("connection"));
        QCOMPARE(e.address, QStringLiteral("::ffff:172.18.0.3"));

        e = entry(QStringLiteral("[ERROR] X server -- no display in range (10 to 10) is available"), 3);
        QCOMPARE(e.category, QStringLiteral("session-failure"));

        QCOMPARE(entry(QStringLiteral("[ERROR] xrdp_sec_recv: xrdp_mcs_recv failed"), 3, QStringLiteral("xrdp")).category, QStringLiteral("error"));
        // client cancelling the TLS handshake is not counted as a server encryption problem
        QCOMPARE(entry(QStringLiteral("[ERROR] SSL_accept: Failure in SSL library (protocol error?)"), 3, QStringLiteral("xrdp")).category, QStringLiteral("connection"));

        // one wrong password produces several lines; only AUTHFAIL counts, and reads plainly
        QVERIFY(entry(QStringLiteral("[INFO ] AUTHFAIL: user=bob ip=::ffff:10.0.0.7 time=1")).primary);
        QVERIFY(!entry(QStringLiteral("[ERROR] pam_authenticate failed: Authentication failure")).primary);
        QVERIFY(!entry(QStringLiteral("[INFO ] login failed for user bob"), 6, QStringLiteral("xrdp")).primary);
        QCOMPARE(entry(QStringLiteral("[INFO ] AUTHFAIL: user=bob ip=::ffff:10.0.0.7 time=1")).plain,
                 QStringLiteral("Wrong user name or password for \"bob\" (from 10.0.0.7)"));
        QCOMPARE(entry(QStringLiteral("[INFO ] login denied for user eve")).plain,
                 QStringLiteral("\"eve\" tried to connect but is not on the allowed list"));
        QCOMPARE(entry(QStringLiteral("Started xrdp.service - xrdp daemon."), 6, QStringLiteral("systemd")).category, QStringLiteral("service"));
    }

    void parsesRealJournalCapture()
    {
        QFile f(QStringLiteral(URDM_FIXTURE_DIR "/journal-xrdp-0.9.24.jsonl"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        int parsed = 0, starts = 0, failures = 0;
        while (!f.atEnd()) {
            const auto e = LogClassifier::parseJournalJson(f.readLine());
            QVERIFY(e.has_value());
            QVERIFY(e->timeMs > 1700000000000LL);
            QVERIFY(!e->cursor.isEmpty());
            ++parsed;
            starts += e->category == QLatin1String("session-start");
            failures += e->category == QLatin1String("auth-failure");
        }
        QVERIFY(parsed > 100);
        QVERIFY(starts >= 1);
        QVERIFY(failures >= 1);
    }

    void binaryMessage()
    {
        const auto e = LogClassifier::parseJournalJson(R"({"__REALTIME_TIMESTAMP":"1792819030000000","PRIORITY":"3","SYSLOG_IDENTIFIER":"xrdp","MESSAGE":[104,105],"__CURSOR":"c"})");
        QVERIFY(e);
        QCOMPARE(e->message, QStringLiteral("hi"));
        QVERIFY(!LogClassifier::parseJournalJson("not json"));
    }
};

QTEST_GUILESS_MAIN(TstLogClassifier)
#include "tst_logclassifier.moc"
