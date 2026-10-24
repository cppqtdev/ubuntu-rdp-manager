#include "HelperCore.h"
#include "HelperProtocol.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace urdm;
using namespace urdm::helper;

namespace {

struct RecordedCommand
{
    QString program;
    QStringList args;
    QHash<QString, QString> env;
};

class FakeSystemOps : public SystemOps
{
public:
    QString xrdpVersion = QStringLiteral("0.9.24-4ubuntu0.1");
    QHash<QString, QVariantMap> sessions;
    QHash<QString, GroupInfo> groups;
    QHash<QString, UserInfo> users;
    QHash<QString, FileOwnership> files;
    QHash<QString, QByteArray> contents;
    QList<RecordedCommand> commands;
    QStringList terminated;
    QString loadState = QStringLiteral("loaded");
    QStringList jobs;
    QList<QPair<QStringList, bool>> unitFileChanges;
    std::function<CommandResult(const RecordedCommand &)> onRun;

    CommandResult run(const QString &program, const QStringList &args, const QHash<QString, QString> &extraEnv, int) override
    {
        RecordedCommand c{program, args, extraEnv};
        commands << c;
        if (onRun)
            return onRun(c);
        CommandResult r;
        r.exitCode = 0;
        return r;
    }
    QString packageVersion(const QString &pkg) override { return pkg == QLatin1String("xrdp") ? xrdpVersion : QString(); }
    bool fileExists(const QString &path) override { return files.contains(path); }
    FileOwnership stat(const QString &path) override { return files.value(path); }
    std::optional<QByteArray> readFile(const QString &path, qint64) override
    {
        if (!contents.contains(path))
            return std::nullopt;
        return contents.value(path);
    }
    bool writeFile(const QString &path, const QByteArray &data, unsigned uid, unsigned gid, unsigned mode, QString *) override
    {
        contents.insert(path, data);
        FileOwnership o;
        o.exists = o.regularFile = true;
        o.uid = uid;
        o.gid = gid;
        o.mode = mode;
        o.size = data.size();
        files.insert(path, o);
        return true;
    }
    GroupInfo group(const QString &name) override { return groups.value(name); }
    UserInfo user(const QString &name) override { return users.value(name); }
    QList<unsigned> supplementaryGroups(const QString &) override { return {1000}; }
    QString unitLoadState(const QString &) override { return loadState; }
    void unitJob(const QString &method, const QString &unit, Completion done) override
    {
        jobs << method + QLatin1Char(' ') + unit;
        done(true, QStringLiteral("ok"));
    }
    bool setUnitFilesEnabled(const QStringList &units, bool enabled, QString *) override
    {
        unitFileChanges << qMakePair(units, enabled);
        return true;
    }
    std::optional<QVariantMap> sessionProperties(const QString &id) override
    {
        if (!sessions.contains(id))
            return std::nullopt;
        return sessions.value(id);
    }
    bool terminateSession(const QString &id, QString *) override
    {
        terminated << id;
        return true;
    }
    std::optional<QVariantMap> xrdpClientAddresses(QString *) override { return QVariantMap{{QStringLiteral("10"), QStringLiteral("10.0.0.5:51234")}}; }
};

FileOwnership regular(unsigned uid, unsigned gid, unsigned mode, qint64 size = 100)
{
    FileOwnership o;
    o.exists = o.regularFile = true;
    o.uid = uid;
    o.gid = gid;
    o.mode = mode;
    o.size = size;
    return o;
}

} // namespace

class TstHelperCore : public QObject
{
    Q_OBJECT

    QTemporaryDir m_tmp;
    HelperPaths m_paths;
    std::unique_ptr<FakeSystemOps> m_ops;
    std::unique_ptr<HelperCore> m_core;

    QString sesmanText() const
    {
        QFile f(m_paths.xrdpConfigDir + QStringLiteral("/sesman.ini"));
        f.open(QIODevice::ReadOnly);
        return QString::fromUtf8(f.readAll());
    }

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        const QString base = m_tmp.path() + QStringLiteral("/") + QString::fromLatin1(QTest::currentTestFunction());
        m_paths.xrdpConfigDir = base + QStringLiteral("/etc-xrdp");
        m_paths.managedCertDir = base + QStringLiteral("/etc-xrdp/urdm");
        m_paths.backupDir = base + QStringLiteral("/backups");
        m_paths.startwmDir = base + QStringLiteral("/startwm");
        m_paths.xrdpSockDir = QStringLiteral("/run/xrdp/sockdir");
        QDir().mkpath(m_paths.xrdpConfigDir);
        for (const char *n : {"sesman.ini", "xrdp.ini"})
            QVERIFY(QFile::copy(QStringLiteral(URDM_FIXTURE_DIR "/") + QString::fromLatin1(n) + QStringLiteral(".ubuntu-24.04"),
                                m_paths.xrdpConfigDir + QLatin1Char('/') + QString::fromLatin1(n)));
        m_ops = std::make_unique<FakeSystemOps>();
        m_core = std::make_unique<HelperCore>(*m_ops, m_paths);

        m_ops->sessions.insert(QStringLiteral("c1"), {{QStringLiteral("Service"), QStringLiteral("xrdp-sesman")},
                                                      {QStringLiteral("Display"), QStringLiteral(":10")},
                                                      {QStringLiteral("Name"), QStringLiteral("alice")}});
        m_ops->sessions.insert(QStringLiteral("3"), {{QStringLiteral("Service"), QStringLiteral("sshd")},
                                                     {QStringLiteral("Display"), QString()},
                                                     {QStringLiteral("Name"), QStringLiteral("bob")}});
        m_ops->files.insert(QStringLiteral("/run/xrdp/sockdir/xrdp_disconnect_display_10"), regular(1001, 103, 0660, 0));
        m_ops->users.insert(QStringLiteral("alice"), {true, 1001, 1001});
        m_ops->users.insert(QStringLiteral("bob"), {true, 1002, 1002});
        m_ops->users.insert(QStringLiteral("daemon"), {true, 1, 1});
        m_ops->groups.insert(QStringLiteral("xrdp"), {true, 103, {}});
    }

    // ---------------- service control ----------------
    void serviceActionsAreWhitelisted()
    {
        OpResult result;
        m_core->controlService(QStringLiteral("start; rm -rf /"), [&](OpResult r) { result = r; });
        QVERIFY(!result.ok);
        QCOMPARE(result.errorName, protocol::kErrInvalidArgument);
        QVERIFY(m_ops->jobs.isEmpty());
    }

    void startStopTargetXrdpUnit()
    {
        OpResult result;
        m_core->controlService(QStringLiteral("restart"), [&](OpResult r) { result = r; });
        QVERIFY(result.ok);
        QCOMPARE(m_ops->jobs, QStringList{QStringLiteral("RestartUnit xrdp.service")});
        m_core->controlService(QStringLiteral("disable"), [&](OpResult r) { result = r; });
        QVERIFY(result.ok);
        QCOMPARE(m_ops->unitFileChanges.size(), 1);
        QCOMPARE(m_ops->unitFileChanges.first().first, (QStringList{QStringLiteral("xrdp.service"), QStringLiteral("xrdp-sesman.service")}));
        QCOMPARE(m_ops->unitFileChanges.first().second, false);
    }

    void serviceControlRequiresInstalledUnit()
    {
        m_ops->loadState = QStringLiteral("not-found");
        OpResult result;
        m_core->controlService(QStringLiteral("start"), [&](OpResult r) { result = r; });
        QVERIFY(!result.ok);
        QVERIFY(result.message.contains(QStringLiteral("not installed")));
    }

    // ---------------- sessions ----------------
    void disconnectUsesXrdpDisWithDisplay()
    {
        m_ops->onRun = [](const RecordedCommand &) {
            CommandResult r;
            r.exitCode = 0;
            r.stdOut = "message sent ok\n";
            return r;
        };
        const auto r = m_core->disconnectSession(QStringLiteral("c1"));
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(m_ops->commands.size(), 1);
        QCOMPARE(m_ops->commands.first().program, QStringLiteral("/usr/bin/xrdp-dis"));
        QVERIFY(m_ops->commands.first().args.isEmpty());
        QCOMPARE(m_ops->commands.first().env.value(QStringLiteral("DISPLAY")), QStringLiteral(":10"));
    }

    void nonXrdpSessionsAreNeverTouched()
    {
        auto r = m_core->terminateSession(QStringLiteral("3"));
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("not an xrdp session")));
        r = m_core->disconnectSession(QStringLiteral("3"));
        QVERIFY(!r.ok);
        QVERIFY(m_ops->terminated.isEmpty());
        QVERIFY(m_ops->commands.isEmpty());
    }

    void sessionIdsAreValidated()
    {
        QVERIFY(!m_core->terminateSession(QStringLiteral("c1; reboot")).ok);
        QVERIFY(!m_core->terminateSession(QStringLiteral("../c1")).ok);
        QVERIFY(!m_core->terminateSession(QStringLiteral("c99")).ok);
        QVERIFY(m_core->terminateSession(QStringLiteral("c1")).ok);
        QCOMPARE(m_ops->terminated, QStringList{QStringLiteral("c1")});
    }

    // ---------------- access control ----------------
    void restrictionRefusesEmptyGroup()
    {
        const auto r = m_core->setAccessRestriction(true, QStringLiteral("rdpusers"), {});
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("nobody could log in")));
        QVERIFY(m_ops->commands.isEmpty());
    }

    void restrictionCreatesGroupAddsMembersAndConfigures()
    {
        const auto r = m_core->setAccessRestriction(true, QStringLiteral("rdpusers"), {QStringLiteral("alice")});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(m_ops->commands.size(), 2);
        QCOMPARE(m_ops->commands.at(0).program, QStringLiteral("/usr/sbin/groupadd"));
        QCOMPARE(m_ops->commands.at(0).args, (QStringList{QStringLiteral("--"), QStringLiteral("rdpusers")}));
        QCOMPARE(m_ops->commands.at(1).args, (QStringList{QStringLiteral("-a"), QStringLiteral("alice"), QStringLiteral("rdpusers")}));
        const QString ini = sesmanText();
        QVERIFY(ini.contains(QStringLiteral("TerminalServerUsers=rdpusers")));
        QVERIFY(ini.contains(QStringLiteral("AlwaysGroupCheck=true")));
    }

    void disablingRestrictionRemovesKey()
    {
        const auto r = m_core->setAccessRestriction(false, {}, {});
        QVERIFY2(r.ok, qPrintable(r.message));
        const QString ini = sesmanText();
        QVERIFY(!ini.contains(QStringLiteral("\nTerminalServerUsers=")));
        QVERIFY(ini.contains(QStringLiteral("AlwaysGroupCheck=false")));
    }

    void userAccessUsesGroupFromConfigAndPreventsLockout()
    {
        // Ubuntu file names "tsusers" which does not exist yet
        auto r = m_core->setUserAccess(QStringLiteral("alice"), true);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("not enabled")));

        m_ops->groups.insert(QStringLiteral("tsusers"), {true, 2000, {QStringLiteral("alice")}});
        r = m_core->setUserAccess(QStringLiteral("alice"), false);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("last member")));

        r = m_core->setUserAccess(QStringLiteral("bob"), true);
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(m_ops->commands.last().args, (QStringList{QStringLiteral("-a"), QStringLiteral("bob"), QStringLiteral("tsusers")}));

        QVERIFY(!m_core->setUserAccess(QStringLiteral("daemon"), true).ok); // system account
        QVERIFY(!m_core->setUserAccess(QStringLiteral("Bob;x"), true).ok);
        QVERIFY(!m_core->setUserAccess(QStringLiteral("nosuchuser"), true).ok);
    }

    // ---------------- certificates ----------------
    void certificatePathsAreChecked()
    {
        Caller alice{QStringLiteral(":1.5"), 1001, QStringLiteral("alice")};
        const QString cert = QStringLiteral("/home/alice/cert.pem");
        const QString key = QStringLiteral("/home/alice/key.pem");

        QVERIFY(!m_core->installCertificate(alice, QStringLiteral("relative.pem"), key).ok);
        QVERIFY(!m_core->installCertificate(alice, QStringLiteral("/home/alice/../root/key.pem"), key).ok);

        m_ops->files.insert(cert, regular(1001, 1001, 0644));
        FileOwnership link = regular(1001, 1001, 0777);
        link.symlink = true;
        link.regularFile = false;
        m_ops->files.insert(key, link);
        auto r = m_core->installCertificate(alice, cert, key);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("symbolic links")));

        // a root-only key the caller cannot read must not be copied (confused deputy)
        m_ops->files.insert(key, regular(0, 0, 0600));
        r = m_core->installCertificate(alice, cert, key);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("read permission")));
        QVERIFY(m_ops->commands.isEmpty());
    }

    void certificateIsInstalledWithRestrictiveKeyPermissions()
    {
        Caller alice{QStringLiteral(":1.5"), 1001, QStringLiteral("alice")};
        const QString cert = QStringLiteral("/home/alice/cert.pem");
        const QString key = QStringLiteral("/home/alice/key.pem");
        m_ops->files.insert(cert, regular(1001, 1001, 0644));
        m_ops->files.insert(key, regular(1001, 1001, 0600));
        m_ops->contents.insert(cert, "-----BEGIN CERTIFICATE-----\nAAA\n-----END CERTIFICATE-----\n");
        m_ops->contents.insert(key, "-----BEGIN PRIVATE KEY-----\nBBB\n-----END PRIVATE KEY-----\n");
        m_ops->onRun = [](const RecordedCommand &c) {
            CommandResult r;
            r.exitCode = 0;
            if (c.args.contains(QStringLiteral("-pubkey")) || c.args.contains(QStringLiteral("-pubout")))
                r.stdOut = "-----BEGIN PUBLIC KEY-----\nSAME\n-----END PUBLIC KEY-----\n";
            return r;
        };
        QDir().mkpath(m_paths.managedCertDir);
        const auto r = m_core->installCertificate(alice, cert, key);
        QVERIFY2(r.ok, qPrintable(r.message));
        const FileOwnership k = m_ops->files.value(m_paths.managedCertDir + QStringLiteral("/key.pem"));
        QCOMPARE(k.uid, 0u);
        QCOMPARE(k.gid, 103u); // xrdp group
        QCOMPARE(k.mode, 0640u);
        QFile xrdpIni(m_paths.xrdpConfigDir + QStringLiteral("/xrdp.ini"));
        QVERIFY(xrdpIni.open(QIODevice::ReadOnly));
        const QByteArray ini = xrdpIni.readAll();
        QVERIFY(ini.contains("certificate=" + (m_paths.managedCertDir + QStringLiteral("/cert.pem")).toUtf8()));
        QVERIFY(ini.contains("key_file=" + (m_paths.managedCertDir + QStringLiteral("/key.pem")).toUtf8()));
    }

    void mismatchedKeyIsRejected()
    {
        Caller root{QStringLiteral(":1.5"), 0, QStringLiteral("root")};
        const QString cert = QStringLiteral("/root/c.pem");
        const QString key = QStringLiteral("/root/k.pem");
        m_ops->files.insert(cert, regular(0, 0, 0600));
        m_ops->files.insert(key, regular(0, 0, 0600));
        m_ops->contents.insert(cert, "-----BEGIN CERTIFICATE-----\n");
        m_ops->contents.insert(key, "-----BEGIN PRIVATE KEY-----\n");
        int n = 0;
        m_ops->onRun = [&n](const RecordedCommand &) {
            CommandResult r;
            r.exitCode = 0;
            r.stdOut = QByteArray("pub") + QByteArray::number(n++);
            return r;
        };
        const auto r = m_core->installCertificate(root, cert, key);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("does not match")));
    }

    void selfSignedValidatesHostName()
    {
        QVERIFY(!m_core->generateSelfSignedCertificate(QStringLiteral("bad host; rm"), 365).ok);
        QVERIFY(!m_core->generateSelfSignedCertificate(QStringLiteral("rdp.example.com"), 5000).ok);
        QVERIFY(m_ops->commands.isEmpty());
    }

    // ---------------- desktop / packages / config ----------------
    void desktopScriptMustBeRootOwned()
    {
        m_ops->files.insert(m_paths.startXfce, regular(0, 0, 0755));
        const QString script = m_paths.startwmDir + QStringLiteral("/startwm-xfce.sh");
        m_ops->files.insert(script, regular(1001, 1001, 0777));
        QVERIFY(!m_core->configureDesktop(QStringLiteral("xfce")).ok);
        m_ops->files.insert(script, regular(0, 0, 0755));
        const auto r = m_core->configureDesktop(QStringLiteral("xfce"));
        QVERIFY2(r.ok, qPrintable(r.message));
        QVERIFY(sesmanText().contains(QStringLiteral("DefaultWindowManager=") + script));
        QVERIFY(!m_core->configureDesktop(QStringLiteral("/bin/sh")).ok);
    }

    void installPlanIsFixed()
    {
        auto r = m_core->buildInstallPlan({QStringLiteral("xrdp"), QStringLiteral("xorgxrdp"), QStringLiteral("xfce")});
        QVERIFY(r.ok);
        const QVariantList plan = r.value.toList();
        QCOMPARE(plan.size(), 2);
        QCOMPARE(plan.at(0).toStringList(), (QStringList{QStringLiteral("/usr/bin/apt-get"), QStringLiteral("update")}));
        const QStringList install = plan.at(1).toStringList();
        QVERIFY(install.contains(QStringLiteral("--force-confold")) || install.contains(QStringLiteral("Dpkg::Options::=--force-confold")));
        QCOMPARE(install.mid(install.indexOf(QStringLiteral("--")) + 1),
                 (QStringList{QStringLiteral("xrdp"), QStringLiteral("xorgxrdp"), QStringLiteral("xfce4"),
                              QStringLiteral("xfce4-terminal"), QStringLiteral("dbus-x11")}));
        QVERIFY(!m_core->buildInstallPlan({QStringLiteral("xrdp"), QStringLiteral("netcat")}).ok);
        QVERIFY(!m_core->buildInstallPlan({}).ok);
    }

    void configChangesUseDetectedVersion()
    {
        auto r = m_core->applyConfigChanges(QStringLiteral("sesman.ini"),
                                            {{QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QStringLiteral("0")}});
        QVERIFY(!r.ok); // 0.9.24 -> 0 denies all
        m_ops->xrdpVersion = QStringLiteral("0.10.4.1-1");
        r = m_core->applyConfigChanges(QStringLiteral("sesman.ini"),
                                       {{QStringLiteral("Sessions"), QStringLiteral("MaxSessions"), QStringLiteral("0")}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QVERIFY(!m_core->applyConfigChanges(QStringLiteral("../../shadow"), {{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}}).ok);
    }

    void journalArgumentsAreBounded()
    {
        QVERIFY(!m_core->readJournal(0, 0).ok);
        QVERIFY(!m_core->readJournal(100000, 0).ok);
        QVERIFY(m_core->readJournal(200, 1700000000).ok);
        const auto &c = m_ops->commands.last();
        QCOMPARE(c.program, QStringLiteral("/usr/bin/journalctl"));
        QVERIFY(c.args.contains(QStringLiteral("@1700000000")));
    }
};

QTEST_GUILESS_MAIN(TstHelperCore)
#include "tst_helpercore.moc"
