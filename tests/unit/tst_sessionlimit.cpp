#include "ConfigSchema.h"
#include "SessionLimitPolicy.h"

#include <QTest>

using namespace urdm;

class TstSessionLimit : public QObject
{
    Q_OBJECT

    const XrdpVersion v0924 = XrdpVersion::fromDebianVersion(QStringLiteral("0.9.24-4ubuntu0.1"));
    const XrdpVersion v0104 = XrdpVersion::fromDebianVersion(QStringLiteral("0.10.4.1-1"));

private slots:
    void parsesDebianVersions()
    {
        QCOMPARE(v0924.toString(), QStringLiteral("0.9.24"));
        QCOMPARE(v0104.toString(), QStringLiteral("0.10.4"));
        QCOMPARE(XrdpVersion::fromDebianVersion(QStringLiteral("1:0.9.19-1")).toString(), QStringLiteral("0.9.19"));
        QVERIFY(!XrdpVersion::fromDebianVersion(QStringLiteral("garbage")).valid);
        QVERIFY(v0924 < v0104);
    }

    // Verified empirically on 2026-10-24: xrdp 0.9.24 denies every login with MaxSessions=0.
    void zeroDeniesAllLoginsOn09()
    {
        const SessionLimitPolicy p(v0924);
        QVERIFY(!p.zeroMeansUnlimited());
        QCOMPARE(p.interpret(QStringLiteral("0")).kind, SessionLimitPolicy::Kind::DeniesAllLogins);
        QCOMPARE(p.interpret(QString()).kind, SessionLimitPolicy::Kind::DeniesAllLogins); // absent key -> default 0
        QCOMPARE(p.unlimitedConfigValue(), SessionLimitPolicy::kLegacyUnlimitedSentinel);
        QCOMPARE(*p.toConfigValue(true, 0), 9999);
    }

    void zeroIsUnlimitedOn010()
    {
        const SessionLimitPolicy p(v0104);
        QVERIFY(p.zeroMeansUnlimited());
        QCOMPARE(p.interpret(QStringLiteral("0")).kind, SessionLimitPolicy::Kind::Unlimited);
        QCOMPARE(*p.toConfigValue(true, 0), 0);
    }

    void unknownVersionIsConservative()
    {
        const SessionLimitPolicy p(XrdpVersion{});
        QVERIFY(!p.zeroMeansUnlimited());
        QCOMPARE(*p.toConfigValue(true, 0), 9999);
    }

    void interpretsLimitsAndSentinel()
    {
        const SessionLimitPolicy p(v0924);
        auto s = p.interpret(QStringLiteral("50"));
        QCOMPARE(s.kind, SessionLimitPolicy::Kind::Limited);
        QCOMPARE(s.limit, 50);
        s = p.interpret(QStringLiteral("9999"));
        QCOMPARE(s.kind, SessionLimitPolicy::Kind::Unlimited);
        QVERIFY(s.description.contains(QStringLiteral("hardware")));
        QCOMPARE(p.interpret(QStringLiteral("-3")).kind, SessionLimitPolicy::Kind::Invalid);
        QCOMPARE(p.interpret(QStringLiteral("abc")).kind, SessionLimitPolicy::Kind::Invalid);
    }

    void validatesExplicitLimits()
    {
        const SessionLimitPolicy p(v0924);
        QString err;
        QCOMPARE(*p.toConfigValue(false, 25), 25);
        QVERIFY(!p.toConfigValue(false, 0, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!p.toConfigValue(false, 9999));
    }

    void schemaRejectsZeroOn09ButAcceptsOn010()
    {
        const SettingDef *d = ConfigSchema::find(QStringLiteral("sessions.maxSessions"));
        QVERIFY(d);
        QVERIFY(ConfigSchema::validate(*d, QStringLiteral("0"), v0924).has_value());
        QVERIFY(!ConfigSchema::validate(*d, QStringLiteral("0"), v0104).has_value());
        QVERIFY(!ConfigSchema::validate(*d, QStringLiteral("9999"), v0924).has_value());
        QVERIFY(ConfigSchema::validate(*d, QStringLiteral("10000"), v0924).has_value());
    }
};

QTEST_GUILESS_MAIN(TstSessionLimit)
#include "tst_sessionlimit.moc"
