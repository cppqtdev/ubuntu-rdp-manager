#include "ProcParsers.h"

#include <QFile>
#include <QTest>

using namespace urdm::proc;

class TstProcParsers : public QObject
{
    Q_OBJECT

    static QByteArray fixture(const char *name)
    {
        QFile f(QStringLiteral(URDM_FIXTURE_DIR "/") + QString::fromLatin1(name));
        if (!f.open(QIODevice::ReadOnly))
            qFatal("missing fixture %s", name);
        return f.readAll();
    }

private slots:
    void cpu()
    {
        const auto a = parseCpuTotals("cpu  100 0 100 800 0 0 0 0 0 0\ncpu0 1 2 3 4\n");
        const auto b = parseCpuTotals("cpu  150 0 150 900 0 0 0 0 0 0\n");
        QVERIFY(a && b);
        QCOMPARE(a->total, quint64(1000));
        QCOMPARE(cpuPercent(*a, *b), 50.0);
        QCOMPARE(cpuPercent(*b, *a), 0.0);
        QVERIFY(!parseCpuTotals("garbage"));
    }

    void meminfoLoadPsiUptime()
    {
        const auto m = parseMemInfo("MemTotal:       15745024 kB\nMemFree: 1 kB\nMemAvailable:    7351296 kB\nSwapTotal: 4194300 kB\nSwapFree: 4194300 kB\n");
        QVERIFY(m);
        QCOMPARE(m->totalKiB, quint64(15745024));
        QCOMPARE(m->availableKiB, quint64(7351296));
        QVERIFY(!parseMemInfo("MemTotal: 1 kB\n"));
        const auto l = parseLoadAvg("2.76 1.67 1.23 3/3158 4094\n");
        QVERIFY(l);
        QCOMPARE(l->one, 2.76);
        QCOMPARE(parsePressureSomeAvg10("some avg10=12.50 avg60=1.00 avg300=0.00 total=1\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0\n").value_or(-1), 12.5);
        QCOMPARE(parseUptimeSeconds("12345.67 999.0\n").value_or(-1), 12345.67);
    }

    // Captured from the test server with two RDP clients attached (displays 10 and 11).
    void attachedDisplaysFromRealProcNetUnix()
    {
        const auto socks = parseProcNetUnix(fixture("proc-net-unix.two-attached"));
        QVERIFY(socks.size() > 100);
        auto d = attachedXrdpDisplays(socks);
        std::sort(d.begin(), d.end());
        QCOMPARE(d, (QList<int>{10, 11}));
    }

    void listeningSocketsFromRealProcNetTcp6()
    {
        const auto socks = parseProcNetTcp(fixture("proc-net-tcp6.xrdp"), true);
        bool rdpListen = false, sesmanListen = false, established = false;
        for (const auto &s : socks) {
            if (s.state == kTcpListen && s.localPort == 3389 && s.localAddress == QLatin1String("::"))
                rdpListen = true;
            if (s.state == kTcpListen && s.localPort == 3350 && s.localAddress == QLatin1String("::1"))
                sesmanListen = true;
            if (s.state == kTcpEstablished && s.localPort == 3389 && s.remoteAddress.startsWith(QLatin1String("172.18.0.")))
                established = true; // IPv4-mapped peer decoded to plain IPv4
        }
        QVERIFY(rdpListen);
        QVERIFY(sesmanListen);
        QVERIFY(established);
    }

    void ipv4Decoding()
    {
        const QByteArray tcp = "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode\n"
                               "   0: 0100007F:0D3D 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 12345 1\n";
        const auto socks = parseProcNetTcp(tcp, false);
        QCOMPARE(socks.size(), 1);
        QCOMPARE(socks.first().localAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(socks.first().localPort, 3389);
        QCOMPARE(socks.first().inode, quint64(12345));
    }

    void pidStatWithSpacesInComm()
    {
        const QByteArray stat = "4247 (dbus daemon (x)) S 1 4247 4247 0 -1 4194560 1000 0 0 0 25 13 0 0 20 0 1 0 98765 10000000 2048 18446744073709551615 0 0 0 0 0 0 0 4096 16384 0 0 0 17 3 0 0 0 0 0";
        const auto s = parsePidStat(stat);
        QVERIFY(s);
        QCOMPARE(s->pid, 4247);
        QCOMPARE(s->comm, QStringLiteral("dbus daemon (x)"));
        QCOMPARE(s->ppid, 1);
        QCOMPARE(s->utimeTicks, quint64(25));
        QCOMPARE(s->stimeTicks, quint64(13));
        QCOMPARE(s->startTimeTicks, quint64(98765));
        QCOMPARE(s->rssPages, quint64(2048));
    }

    void cgroupCpu()
    {
        QCOMPARE(parseCgroupCpuUsageUsec("usage_usec 123456\nuser_usec 1\n").value_or(0), quint64(123456));
        QVERIFY(!parseCgroupCpuUsageUsec("nothing"));
    }

private:
    static std::optional<ProcStat> parsePidStat(const QByteArray &b) { return parseProcPidStat(b); }
};

QTEST_GUILESS_MAIN(TstProcParsers)
#include "tst_procparsers.moc"
