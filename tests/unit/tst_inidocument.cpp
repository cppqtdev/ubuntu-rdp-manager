#include "IniDocument.h"

#include <QFile>
#include <QTest>

using urdm::IniDocument;

class TstIniDocument : public QObject
{
    Q_OBJECT

private:
    static QByteArray fixture(const char *name)
    {
        QFile f(QStringLiteral(URDM_FIXTURE_DIR "/") + QString::fromLatin1(name));
        if (!f.open(QIODevice::ReadOnly))
            qFatal("missing fixture %s", name);
        return f.readAll();
    }

private slots:
    void roundTripUbuntuFilesIsByteExact_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("sesman.ini") << fixture("sesman.ini.ubuntu-24.04");
        QTest::newRow("xrdp.ini") << fixture("xrdp.ini.ubuntu-24.04");
    }
    void roundTripUbuntuFilesIsByteExact()
    {
        QFETCH(QByteArray, data);
        QCOMPARE(IniDocument::parse(data).serialize(), data);
    }

    void readsUbuntuDefaults()
    {
        const auto doc = IniDocument::parse(fixture("sesman.ini.ubuntu-24.04"));
        QCOMPARE(doc.value(QStringLiteral("Sessions"), QStringLiteral("MaxSessions")), QStringLiteral("50"));
        QCOMPARE(doc.value(QStringLiteral("sessions"), QStringLiteral("maxsessions")), QStringLiteral("50")); // case-insensitive
        QCOMPARE(doc.value(QStringLiteral("Sessions"), QStringLiteral("Policy")), QStringLiteral("Default"));
        QCOMPARE(doc.value(QStringLiteral("Security"), QStringLiteral("TerminalServerUsers")), QStringLiteral("tsusers"));
        // duplicate keys are preserved in order
        const QStringList params = doc.values(QStringLiteral("Xorg"), QStringLiteral("param"));
        QVERIFY(params.size() >= 8);
        QCOMPARE(params.first(), QStringLiteral("/usr/lib/xorg/Xorg"));
    }

    void setValueChangesOnlyTargetLine()
    {
        const QByteArray original = fixture("sesman.ini.ubuntu-24.04");
        auto doc = IniDocument::parse(original);
        doc.setValue(QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QStringLiteral("9999"));
        const QByteArray out = doc.serialize();
        QCOMPARE(out.size(), original.size() + 2);
        QVERIFY(out.contains("\nMaxSessions=9999\n"));
        QVERIFY(!out.contains("MaxSessions=50"));
        QVERIFY(out.contains(";; Policy - session allocation policy")); // comments survive
    }

    void setValueAppendsInsideSection()
    {
        auto doc = IniDocument::parse("[A]\nx=1\n\n; comment about B\n[B]\ny=2\n");
        doc.setValue(QStringLiteral("A"), QStringLiteral("z"), QStringLiteral("3"));
        QCOMPARE(doc.serialize(), QByteArray("[A]\nx=1\nz=3\n\n; comment about B\n[B]\ny=2\n"));
    }

    void setValueCreatesMissingSection()
    {
        auto doc = IniDocument::parse("[A]\nx=1\n");
        doc.setValue(QStringLiteral("Channels"), QStringLiteral("rdpdr"), QStringLiteral("false"));
        QCOMPARE(doc.serialize(), QByteArray("[A]\nx=1\n\n[Channels]\nrdpdr=false\n"));
    }

    void keepsOriginalKeySpelling()
    {
        auto doc = IniDocument::parse("[Sessions]\nmaxsessions=5\n");
        doc.setValue(QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QStringLiteral("6"));
        QCOMPARE(doc.serialize(), QByteArray("[Sessions]\nmaxsessions=6\n"));
    }

    void preservesCrLfAndMissingTrailingNewline()
    {
        const QByteArray in("[A]\r\nx=1\r\ny=2");
        auto doc = IniDocument::parse(in);
        QCOMPARE(doc.serialize(), in);
        doc.setValue(QStringLiteral("A"), QStringLiteral("x"), QStringLiteral("5"));
        QCOMPARE(doc.serialize(), QByteArray("[A]\r\nx=5\r\ny=2"));
    }

    void removeKeyRemovesAllOccurrences()
    {
        auto doc = IniDocument::parse("[X]\np=1\np=2\nq=3\n");
        QCOMPARE(doc.removeKey(QStringLiteral("X"), QStringLiteral("P")), 2);
        QCOMPARE(doc.serialize(), QByteArray("[X]\nq=3\n"));
    }
};

QTEST_GUILESS_MAIN(TstIniDocument)
#include "tst_inidocument.moc"
