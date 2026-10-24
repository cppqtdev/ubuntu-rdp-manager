// LogSanitizer, ResourceAdvisor, ServiceState
#include "LogSanitizer.h"
#include "ResourceAdvisor.h"
#include "ServiceState.h"

#include <QTest>

using namespace urdm;

class TstMisc : public QObject
{
    Q_OBJECT

private slots:
    void sanitizerRedactsSecrets_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("secret");
        QTest::newRow("kv") << "connect password=hunter2 ok" << "hunter2";
        QTest::newRow("kv colon") << "Password: \"s3cr et\"" << "s3cr et";
        QTest::newRow("freerdp") << "xfreerdp3 /v:host /u:bob /p:Pa55word /cert:tofu" << "Pa55word";
        QTest::newRow("base64") << "password={base64}c2VjcmV0" << "c2VjcmV0";
        QTest::newRow("token") << "auth_token=abc.def.ghi" << "abc.def.ghi";
        QTest::newRow("bearer") << "Authorization: Bearer eyJhbGciOi" << "eyJhbGciOi";
        QTest::newRow("pem") << "x\n-----BEGIN RSA PRIVATE KEY-----\nMIIEow\nAAAA\n-----END RSA PRIVATE KEY-----\ny" << "MIIEow";
    }
    void sanitizerRedactsSecrets()
    {
        QFETCH(QString, input);
        QFETCH(QString, secret);
        const QString out = LogSanitizer::sanitize(input);
        QVERIFY2(!out.contains(secret), qPrintable(out));
        QVERIFY(out.contains(QStringLiteral("[REDACTED]")));
    }

    void sanitizerKeepsOrdinaryLines()
    {
        const QString line = QStringLiteral("xrdp-sesman[48]: [INFO ] ++ created session (access granted): username rdpuser1, ip ::ffff:172.18.0.3:45832");
        QCOMPARE(LogSanitizer::sanitize(line), line);
    }

    void advisorHealthy()
    {
        ResourceAdvisor::Input in;
        in.memTotalKiB = 16 * 1024 * 1024;
        in.memAvailableKiB = 8 * 1024 * 1024;
        in.cpuCount = 8;
        in.load1 = 1.0;
        in.sessionCount = 10;
        in.measuredSessionMemoryKiB = 10 * 150 * 1024;
        const auto r = ResourceAdvisor::evaluate(in);
        QCOMPARE(r.level, ResourceAdvisor::Level::Ok);
        QVERIFY(r.perSessionMeasured);
        QCOMPARE(r.perSessionEstimateKiB, quint64(150 * 1024));
        // usable = 8 GiB - 1.6 GiB reserve; planning floor 512 MiB -> 12 more sessions
        QCOMPARE(r.estimatedAdditionalSessions, 12);
    }

    void advisorCritical()
    {
        ResourceAdvisor::Input in;
        in.memTotalKiB = 4 * 1024 * 1024;
        in.memAvailableKiB = 100 * 1024;
        in.cpuCount = 2;
        in.load1 = 5.0;
        in.memoryPressureAvg10 = 55.0;
        in.diskFreeBytes = 100ull * 1024 * 1024;
        const auto r = ResourceAdvisor::evaluate(in);
        QCOMPARE(r.level, ResourceAdvisor::Level::Critical);
        QCOMPARE(r.estimatedAdditionalSessions, 0);
        QVERIFY(r.messages.size() >= 4);
    }

    void advisorUnknownMemory()
    {
        const auto r = ResourceAdvisor::evaluate(ResourceAdvisor::Input{});
        QCOMPARE(r.level, ResourceAdvisor::Level::Warning);
    }

    void serviceStateMapping_data()
    {
        QTest::addColumn<QString>("load");
        QTest::addColumn<QString>("active");
        QTest::addColumn<QString>("fileState");
        QTest::addColumn<int>("expected");
        QTest::newRow("running") << "loaded" << "active" << "enabled" << int(ServiceStatus::Running);
        QTest::newRow("stopped") << "loaded" << "inactive" << "disabled" << int(ServiceStatus::Stopped);
        QTest::newRow("starting") << "loaded" << "activating" << "enabled" << int(ServiceStatus::Starting);
        QTest::newRow("stopping") << "loaded" << "deactivating" << "enabled" << int(ServiceStatus::Stopping);
        QTest::newRow("failed") << "loaded" << "failed" << "enabled" << int(ServiceStatus::Failed);
        QTest::newRow("not installed") << "not-found" << "inactive" << "" << int(ServiceStatus::RequiresSetup);
        QTest::newRow("masked") << "masked" << "inactive" << "masked" << int(ServiceStatus::Masked);
        QTest::newRow("unknown") << "" << "" << "" << int(ServiceStatus::Unknown);
    }
    void serviceStateMapping()
    {
        QFETCH(QString, load);
        QFETCH(QString, active);
        QFETCH(QString, fileState);
        QFETCH(int, expected);
        UnitProperties p{load, active, QString(), fileState, QString()};
        QCOMPARE(int(mapServiceStatus(p)), expected);
    }

    void autostart()
    {
        QVERIFY(isAutostartEnabled({QStringLiteral("loaded"), {}, {}, QStringLiteral("enabled"), {}}));
        QVERIFY(!isAutostartEnabled({QStringLiteral("loaded"), {}, {}, QStringLiteral("disabled"), {}}));
    }
};

QTEST_GUILESS_MAIN(TstMisc)
#include "tst_misc.moc"
