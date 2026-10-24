#include "ConfigSchema.h"
#include "ListenSpec.h"
#include "Validators.h"

#include <QTest>

using namespace urdm;

class TstConfigSchema : public QObject
{
    Q_OBJECT
    const XrdpVersion v = XrdpVersion::fromDebianVersion(QStringLiteral("0.9.24-4ubuntu0.1"));

    std::optional<QString> check(const char *id, const char *value)
    {
        const SettingDef *d = ConfigSchema::find(QString::fromLatin1(id));
        if (!d)
            qFatal("unknown setting %s", id);
        return ConfigSchema::validate(*d, QString::fromLatin1(value), v);
    }

private slots:
    void idsAreUniqueAndKeysMapBack()
    {
        QSet<QString> ids;
        for (const auto &d : ConfigSchema::settings()) {
            QVERIFY2(!ids.contains(d.id), qPrintable(d.id));
            ids.insert(d.id);
            QCOMPARE(ConfigSchema::find(d.file, d.section, d.key), &d);
        }
    }

    void boolsIntsEnums()
    {
        QVERIFY(!check("sessions.killDisconnected", "true"));
        QVERIFY(!check("sessions.killDisconnected", "No"));
        QVERIFY(check("sessions.killDisconnected", "maybe"));
        QVERIFY(!check("sessions.idleTimeLimit", "600"));
        QVERIFY(check("sessions.idleTimeLimit", "-1"));
        QVERIFY(check("sessions.idleTimeLimit", "99999999"));
        QVERIFY(!check("sessions.policy", "UBC"));
        QVERIFY(!check("sessions.policy", "default")); // case-insensitive like sesman
        QVERIFY(check("sessions.policy", "Separate")); // 0.10 only
        QVERIFY(!check("redirect.clipboardOutbound", "text,file"));
        QVERIFY(check("redirect.clipboardOutbound", "text,bogus"));
    }

    void clientFeatureSettings()
    {
        // Settings that map to options a connecting client can request.
        for (const char *id : {"redirect.allChannels", "redirect.remoteApps", "redirect.driveMount", "redirect.driveFolder",
                               "display.multiMonitor", "display.bulkCompression", "display.fastPath"})
            QVERIFY2(ConfigSchema::find(QString::fromLatin1(id)), id);
        QCOMPARE(ConfigSchema::find(QStringLiteral("redirect.driveMount"))->section, QStringLiteral("Chansrv"));
        QVERIFY(!check("display.fastPath", "both"));
        QVERIFY(check("display.fastPath", "sometimes"));
        QVERIFY(!check("redirect.driveFolder", "thinclient_drives"));
        QVERIFY(!check("redirect.driveFolder", "/run/user/%u/thinclient_drives"));
        QVERIFY(check("redirect.driveFolder", "../../etc"));
        QVERIFY(check("redirect.driveFolder", "drives with space"));
        QVERIFY(!check("redirect.remoteApps", "false"));
    }

    void secureDefaultsAreEnforced()
    {
        QVERIFY(check("security.layer", "rdp"));
        QVERIFY(!check("security.layer", "tls"));
        QVERIFY(!check("security.tlsProtocols", "TLSv1.2, TLSv1.3"));
        QVERIFY(check("security.tlsProtocols", "TLSv1.1, TLSv1.2"));
        QVERIFY(check("security.tlsProtocols", "SSLv3"));
        QVERIFY(!check("security.tlsCiphers", "HIGH:!aNULL:!MD5"));
        QVERIFY(check("security.tlsCiphers", "HIGH; rm -rf /"));
    }

    void injectionIsRejected()
    {
        QVERIFY(check("sessions.policy", "Default\n[Security]\nAllowRootLogin=true"));
        QVERIFY(check("network.listen", "3389\nfoo=bar"));
    }

    void listenSpecGrammar_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<bool>("exposed");
        QTest::newRow("bare") << "3389" << true << true;
        QTest::newRow("loopback v4") << "tcp://.:3389" << true << false;
        QTest::newRow("all v4") << "tcp://:3389" << true << true;
        QTest::newRow("lan v4") << "tcp://192.168.1.10:3390" << true << true;
        QTest::newRow("loopback v6") << "tcp6://.:3389" << true << false;
        QTest::newRow("v6 literal") << "tcp6://{fd00::1}:3389" << true << true;
        QTest::newRow("multi") << "tcp://127.0.0.1:3389 tcp6://{::1}:3389" << true << false;
        QTest::newRow("unix") << "unix://./tmp/xrdp.socket" << true << false;
        QTest::newRow("bad port") << "70000" << false << false;
        QTest::newRow("bad ip") << "tcp://999.1.1.1:3389" << false << false;
        QTest::newRow("v6 in tcp") << "tcp://::1:3389" << false << false;
        QTest::newRow("junk") << "http://x" << false << false;
        QTest::newRow("empty") << "" << false << false;
    }
    void listenSpecGrammar()
    {
        QFETCH(QString, value);
        QFETCH(bool, valid);
        QFETCH(bool, exposed);
        const auto spec = ListenSpec::parse(value);
        QCOMPARE(spec.has_value(), valid);
        if (spec) {
            QCOMPARE(spec->exposesNonLoopback(), exposed);
            QCOMPARE(ListenSpec::parse(spec->toString())->toString(), spec->toString()); // stable
        }
    }

    void listenSpecHelpers()
    {
        auto s = ListenSpec::singleTcp(QStringLiteral("10.0.0.5"), 3390);
        QCOMPARE(s.toString(), QStringLiteral("tcp://10.0.0.5:3390"));
        QCOMPARE(s.tcpPorts(), QList<int>{3390});
        QCOMPARE(ListenSpec::singleTcp(QString(), 3389).toString(), QStringLiteral("3389"));
        QCOMPARE(ListenSpec::singleTcp(QStringLiteral("fd00::2"), 3389).toString(), QStringLiteral("tcp6://{fd00::2}:3389"));
    }

    void validators()
    {
        using namespace urdm::validators;
        QVERIFY(isValidUserName(QStringLiteral("alice")));
        QVERIFY(isValidUserName(QStringLiteral("rdp_user-2")));
        QVERIFY(!isValidUserName(QStringLiteral("Alice")));
        QVERIFY(!isValidUserName(QStringLiteral("a;rm")));
        QVERIFY(!isValidUserName(QStringLiteral("-rf")));
        QVERIFY(!isValidUserName(QString()));
        QVERIFY(isValidLogindSessionId(QStringLiteral("c12")));
        QVERIFY(!isValidLogindSessionId(QStringLiteral("../1")));
        QCOMPARE(parseDisplayNumber(QStringLiteral(":10")).value_or(-1), 10);
        QCOMPARE(parseDisplayNumber(QStringLiteral(":11.0")).value_or(-1), 11);
        QVERIFY(!parseDisplayNumber(QStringLiteral("10")));
        QVERIFY(!parseDisplayNumber(QStringLiteral(":1;x")));
        QVERIFY(isSafeAbsolutePath(QStringLiteral("/etc/xrdp/cert.pem")));
        QVERIFY(!isSafeAbsolutePath(QStringLiteral("/etc/xrdp/../shadow")));
        QVERIFY(!isSafeAbsolutePath(QStringLiteral("etc/xrdp/cert.pem")));
        QVERIFY(!isSafeAbsolutePath(QStringLiteral("/etc//xrdp")));
        QVERIFY(!isSafeAbsolutePath(QStringLiteral("/etc/x\nrdp")));
        QVERIFY(isValidHostName(QStringLiteral("rdp.example.com")));
        QVERIFY(isValidHostName(QStringLiteral("192.168.1.1")));
        QVERIFY(!isValidHostName(QStringLiteral("bad host")));
    }
};

QTEST_GUILESS_MAIN(TstConfigSchema)
#include "tst_configschema.moc"
