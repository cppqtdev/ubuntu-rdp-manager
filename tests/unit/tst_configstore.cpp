#include "ConfigStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace urdm;

class TstConfigStore : public QObject
{
    Q_OBJECT

    const XrdpVersion v = XrdpVersion::fromDebianVersion(QStringLiteral("0.9.24-4ubuntu0.1"));
    QTemporaryDir m_tmp;
    QString m_etc, m_backups;

    static QByteArray readFile(const QString &p)
    {
        QFile f(p);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_etc = m_tmp.path() + QStringLiteral("/etc-xrdp");
        m_backups = m_tmp.path() + QStringLiteral("/backups");
        QDir().mkpath(m_etc);
        QDir(m_backups).removeRecursively();
        for (const char *n : {"sesman.ini", "xrdp.ini"}) {
            const QString dst = m_etc + QLatin1Char('/') + QString::fromLatin1(n);
            QFile::remove(dst);
            QVERIFY(QFile::copy(QStringLiteral(URDM_FIXTURE_DIR "/") + QString::fromLatin1(n) + QStringLiteral(".ubuntu-24.04"), dst));
            QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
        }
    }

    void writeBacksUpAndApplies()
    {
        ConfigStore store(m_etc, m_backups);
        const QByteArray original = readFile(store.filePath(ConfigFile::SesmanIni));
        QString err;
        const auto id = store.write(ConfigFile::SesmanIni,
                                    {{QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QStringLiteral("9999")},
                                     {QStringLiteral("Sessions"), QStringLiteral("Policy"), QStringLiteral("UBC")}},
                                    v, false, &err);
        QVERIFY2(id, qPrintable(err));
        const auto doc = store.load(ConfigFile::SesmanIni);
        QCOMPARE(doc->value(QStringLiteral("Sessions"), QStringLiteral("MaxSessions")), QStringLiteral("9999"));
        QCOMPARE(doc->value(QStringLiteral("Sessions"), QStringLiteral("Policy")), QStringLiteral("UBC"));
        // backup holds the previous content byte-for-byte and keeps permissions
        const auto backups = store.listBackups(ConfigFile::SesmanIni);
        QCOMPARE(backups.size(), 1);
        QCOMPARE(backups.first().id, *id);
        QCOMPARE(readFile(backups.first().path), original);
        QCOMPARE(QFile::permissions(store.filePath(ConfigFile::SesmanIni)) & QFileDevice::WriteOther, QFileDevice::Permissions());
        QVERIFY(QFile::permissions(store.filePath(ConfigFile::SesmanIni)) & QFileDevice::ReadOther);
    }

    void invalidChangeLeavesFileUntouched()
    {
        ConfigStore store(m_etc, m_backups);
        const QByteArray original = readFile(store.filePath(ConfigFile::SesmanIni));
        QString err;
        QVERIFY(!store.write(ConfigFile::SesmanIni,
                             {{QStringLiteral("Sessions"), QStringLiteral("Policy"), QStringLiteral("UBC")},
                              {QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QStringLiteral("0")}},
                             v, false, &err));
        QVERIFY(err.contains(QStringLiteral("deny every login")));
        QCOMPARE(readFile(store.filePath(ConfigFile::SesmanIni)), original);
        QVERIFY(store.listBackups(ConfigFile::SesmanIni).isEmpty());
    }

    void unknownAndInternalKeysRejected()
    {
        ConfigStore store(m_etc, m_backups);
        QString err;
        QVERIFY(!store.write(ConfigFile::SesmanIni, {{QStringLiteral("Xorg"), QStringLiteral("param"), QStringLiteral("/bin/sh")}}, v, false, &err));
        QVERIFY(err.contains(QStringLiteral("not a supported setting")));
        QVERIFY(!store.write(ConfigFile::SesmanIni,
                             {{QStringLiteral("Globals"), QStringLiteral("DefaultWindowManager"), QStringLiteral("/tmp/evil.sh")}},
                             v, false, &err));
        QVERIFY(err.contains(QStringLiteral("dedicated operation")));
        // the same key is accepted when the dedicated operation passes allowInternal
        QVERIFY(store.write(ConfigFile::SesmanIni,
                            {{QStringLiteral("Globals"), QStringLiteral("DefaultWindowManager"),
                              QStringLiteral("/opt/ubuntu-rdp-manager/share/startwm/startwm-xfce.sh")}},
                            v, true, &err));
    }

    void restoreRollsBack()
    {
        ConfigStore store(m_etc, m_backups);
        const QByteArray original = readFile(store.filePath(ConfigFile::XrdpIni));
        QString err;
        const auto id = store.write(ConfigFile::XrdpIni,
                                    {{QStringLiteral("Globals"), QStringLiteral("security_layer"), QStringLiteral("tls")}}, v,
                                    false, &err);
        QVERIFY2(id, qPrintable(err));
        QVERIFY(readFile(store.filePath(ConfigFile::XrdpIni)) != original);
        QVERIFY2(store.restore(ConfigFile::XrdpIni, *id, &err), qPrintable(err));
        QCOMPARE(readFile(store.filePath(ConfigFile::XrdpIni)), original);
    }

    void restoreRejectsTraversalAndCrossFileIds()
    {
        ConfigStore store(m_etc, m_backups);
        QString err;
        QVERIFY(!store.restore(ConfigFile::SesmanIni, QStringLiteral("../../etc/shadow"), &err));
        QVERIFY(!store.restore(ConfigFile::SesmanIni, QStringLiteral("xrdp.ini.20261024T101010123"), &err));
        QVERIFY(!store.restore(ConfigFile::SesmanIni, QStringLiteral("sesman.ini.20261024T101010123/../x"), &err));
    }

    void prunesOldBackups()
    {
        ConfigStore store(m_etc, m_backups, 3);
        QString err;
        for (int i = 1; i <= 5; ++i) {
            QVERIFY2(store.write(ConfigFile::SesmanIni,
                                 {{QStringLiteral("Sessions"), QStringLiteral("IdleTimeLimit"), QString::number(i * 60)}}, v,
                                 false, &err),
                     qPrintable(err));
            QThread::msleep(2);
        }
        QCOMPARE(store.listBackups(ConfigFile::SesmanIni).size(), 3);
    }

    void missingFileReportsError()
    {
        ConfigStore store(m_tmp.path() + QStringLiteral("/nope"), m_backups);
        QString err;
        QVERIFY(!store.write(ConfigFile::SesmanIni, {{QStringLiteral("Sessions"), QStringLiteral("Policy"), QStringLiteral("UBC")}}, v, false, &err));
        QVERIFY(err.contains(QStringLiteral("Cannot read")));
    }
};

QTEST_GUILESS_MAIN(TstConfigStore)
#include "tst_configstore.moc"
