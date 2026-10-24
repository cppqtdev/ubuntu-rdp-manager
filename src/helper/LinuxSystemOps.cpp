#include "LinuxSystemOps.h"

#include "ProcParsers.h"

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <grp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace urdm::helper {

namespace {
const QString kSystemd = QStringLiteral("org.freedesktop.systemd1");
const QString kSystemdPath = QStringLiteral("/org/freedesktop/systemd1");
const QString kSystemdManager = QStringLiteral("org.freedesktop.systemd1.Manager");
const QString kLogind = QStringLiteral("org.freedesktop.login1");
const QString kLogindPath = QStringLiteral("/org/freedesktop/login1");
const QString kLogindManager = QStringLiteral("org.freedesktop.login1.Manager");
constexpr int kJobTimeoutMs = 120000;

QString errnoText()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}
} // namespace

LinuxSystemOps::LinuxSystemOps(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
}

CommandResult LinuxSystemOps::run(const QString &program, const QStringList &args,
                                  const QHash<QString, QString> &extraEnv, int timeoutMs)
{
    CommandResult r;
    QProcess proc;
    QProcessEnvironment env; // start from an empty environment: nothing inherited from D-Bus activation
    env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin:/sbin:/bin"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    for (auto it = extraEnv.cbegin(); it != extraEnv.cend(); ++it)
        env.insert(it.key(), it.value());
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(QStringLiteral("/"));
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.start(program, args, QIODevice::ReadOnly);
    if (!proc.waitForStarted(10000)) {
        r.startFailed = true;
        r.stdErr = proc.errorString().toUtf8();
        return r;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        r.timedOut = true;
        proc.kill();
        proc.waitForFinished(2000);
    }
    r.stdOut = proc.readAllStandardOutput();
    r.stdErr = proc.readAllStandardError();
    r.exitCode = proc.exitStatus() == QProcess::NormalExit ? proc.exitCode() : -1;
    return r;
}

QString LinuxSystemOps::packageVersion(const QString &package)
{
    const auto r = run(QStringLiteral("/usr/bin/dpkg-query"),
                       {QStringLiteral("-W"), QStringLiteral("-f=${db:Status-Abbrev}|${Version}"), QStringLiteral("--"), package}, {}, 15000);
    if (!r.ok())
        return {};
    const QString out = QString::fromUtf8(r.stdOut).trimmed();
    const qsizetype bar = out.indexOf(QLatin1Char('|'));
    if (bar < 0 || !out.startsWith(QLatin1String("ii")))
        return {};
    return out.mid(bar + 1);
}

bool LinuxSystemOps::fileExists(const QString &path)
{
    struct stat st{};
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0;
}

FileOwnership LinuxSystemOps::stat(const QString &path)
{
    FileOwnership o;
    struct stat st{};
    if (::lstat(QFile::encodeName(path).constData(), &st) != 0)
        return o;
    o.exists = true;
    o.symlink = S_ISLNK(st.st_mode);
    o.regularFile = S_ISREG(st.st_mode);
    o.uid = st.st_uid;
    o.gid = st.st_gid;
    o.mode = st.st_mode & 07777;
    o.size = st.st_size;
    return o;
}

std::optional<QByteArray> LinuxSystemOps::readFile(const QString &path, qint64 maxBytes)
{
    // O_NOFOLLOW: never read through a symlink swapped in after validation.
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return std::nullopt;
    QFile f;
    if (!f.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return std::nullopt;
    }
    const QByteArray data = f.read(maxBytes + 1);
    if (data.size() > maxBytes)
        return std::nullopt;
    return data;
}

bool LinuxSystemOps::writeFile(const QString &path, const QByteArray &data, unsigned uid, unsigned gid, unsigned mode,
                               QString *error)
{
    const QFileInfo fi(path);
    QByteArray tmpl = QFile::encodeName(fi.absolutePath() + QStringLiteral("/.") + fi.fileName() + QStringLiteral(".XXXXXX"));
    const int fd = ::mkostemp(tmpl.data(), O_CLOEXEC);
    if (fd < 0) {
        if (error)
            *error = QStringLiteral("Cannot create temporary file in %1: %2").arg(fi.absolutePath(), errnoText());
        return false;
    }
    auto fail = [&](const QString &what) {
        if (error)
            *error = QStringLiteral("%1 %2: %3").arg(what, path, errnoText());
        ::close(fd);
        ::unlink(tmpl.constData());
        return false;
    };
    // Restrict before writing any content.
    if (::fchmod(fd, 0600) != 0)
        return fail(QStringLiteral("chmod"));
    qint64 written = 0;
    while (written < data.size()) {
        const ssize_t n = ::write(fd, data.constData() + written, size_t(data.size() - written));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return fail(QStringLiteral("write"));
        }
        written += n;
    }
    if (::fchown(fd, uid, gid) != 0)
        return fail(QStringLiteral("chown"));
    if (::fchmod(fd, mode) != 0)
        return fail(QStringLiteral("chmod"));
    if (::fsync(fd) != 0)
        return fail(QStringLiteral("fsync"));
    ::close(fd);
    if (::rename(tmpl.constData(), QFile::encodeName(path).constData()) != 0) {
        if (error)
            *error = QStringLiteral("rename %1: %2").arg(path, errnoText());
        ::unlink(tmpl.constData());
        return false;
    }
    return true;
}

GroupInfo LinuxSystemOps::group(const QString &name)
{
    GroupInfo g;
    struct group grp{};
    struct group *res = nullptr;
    QByteArray buf(16384, '\0');
    if (::getgrnam_r(name.toUtf8().constData(), &grp, buf.data(), size_t(buf.size()), &res) != 0 || !res)
        return g;
    g.exists = true;
    g.gid = res->gr_gid;
    for (char **m = res->gr_mem; m && *m; ++m)
        g.members << QString::fromUtf8(*m);
    return g;
}

UserInfo LinuxSystemOps::user(const QString &name)
{
    UserInfo u;
    struct passwd pw{};
    struct passwd *res = nullptr;
    QByteArray buf(16384, '\0');
    if (::getpwnam_r(name.toUtf8().constData(), &pw, buf.data(), size_t(buf.size()), &res) != 0 || !res)
        return u;
    u.exists = true;
    u.uid = res->pw_uid;
    u.gid = res->pw_gid;
    return u;
}

QList<unsigned> LinuxSystemOps::supplementaryGroups(const QString &userName)
{
    QList<unsigned> result;
    const UserInfo u = user(userName);
    if (!u.exists)
        return result;
    int n = 64;
    QList<gid_t> groups(n);
    const QByteArray nameUtf8 = userName.toUtf8();
    if (::getgrouplist(nameUtf8.constData(), u.gid, groups.data(), &n) < 0) {
        groups.resize(n);
        if (::getgrouplist(nameUtf8.constData(), u.gid, groups.data(), &n) < 0)
            return result;
    }
    for (int i = 0; i < n; ++i)
        result << groups.at(i);
    return result;
}

// ------------------------------------------------------------------------------------------
// systemd
// ------------------------------------------------------------------------------------------
QString LinuxSystemOps::unitLoadState(const QString &unit)
{
    QDBusInterface mgr(kSystemd, kSystemdPath, kSystemdManager, m_bus);
    const QDBusReply<QDBusObjectPath> path = mgr.call(QStringLiteral("LoadUnit"), unit);
    if (!path.isValid())
        return {};
    QDBusInterface props(kSystemd, path.value().path(), QStringLiteral("org.freedesktop.DBus.Properties"), m_bus);
    const QDBusReply<QDBusVariant> v = props.call(QStringLiteral("Get"), QStringLiteral("org.freedesktop.systemd1.Unit"),
                                                  QStringLiteral("LoadState"));
    return v.isValid() ? v.value().variant().toString() : QString();
}

void LinuxSystemOps::unitJob(const QString &method, const QString &unit, Completion done)
{
    if (!m_subscribed) {
        m_bus.connect(kSystemd, kSystemdPath, kSystemdManager, QStringLiteral("JobRemoved"), this,
                      SLOT(onJobRemoved(uint,QDBusObjectPath,QString,QString)));
        QDBusInterface mgr(kSystemd, kSystemdPath, kSystemdManager, m_bus);
        mgr.call(QStringLiteral("Subscribe"));
        m_subscribed = true;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kSystemd, kSystemdPath, kSystemdManager, method);
    msg << unit << QStringLiteral("replace");
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 30000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, method, unit, done]() {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            done(false, QStringLiteral("%1 %2 failed: %3").arg(method, unit, reply.error().message()));
            return;
        }
        const QString job = reply.value().path();
        if (auto early = m_earlyResults.take(job); !early.isEmpty()) {
            PendingJob pj{unit, method, done, nullptr};
            m_jobs.insert(job, pj);
            finishJob(job, early);
            return;
        }
        PendingJob pj{unit, method, done, new QTimer(this)};
        pj.timer->setSingleShot(true);
        connect(pj.timer, &QTimer::timeout, this, [this, job]() { finishJob(job, QStringLiteral("timeout")); });
        pj.timer->start(kJobTimeoutMs);
        m_jobs.insert(job, pj);
    });
}

void LinuxSystemOps::onJobRemoved(uint, const QDBusObjectPath &job, const QString &, const QString &result)
{
    if (m_jobs.contains(job.path())) {
        finishJob(job.path(), result);
    } else {
        if (m_earlyResults.size() > 64)
            m_earlyResults.clear();
        m_earlyResults.insert(job.path(), result);
    }
}

void LinuxSystemOps::finishJob(const QString &jobPath, const QString &result)
{
    PendingJob pj = m_jobs.take(jobPath);
    if (pj.timer)
        pj.timer->deleteLater();
    if (!pj.done)
        return;
    if (result == QLatin1String("done")) {
        pj.done(true, QStringLiteral("%1 completed for %2").arg(pj.method, pj.unit));
    } else {
        // systemd job results: canceled, timeout, failed, dependency, skipped
        pj.done(false, QStringLiteral("%1 for %2 finished with result \"%3\"; see the service log for details")
                           .arg(pj.method, pj.unit, result));
    }
}

bool LinuxSystemOps::setUnitFilesEnabled(const QStringList &units, bool enabled, QString *error)
{
    QDBusInterface mgr(kSystemd, kSystemdPath, kSystemdManager, m_bus);
    QDBusMessage reply = enabled ? mgr.call(QStringLiteral("EnableUnitFiles"), units, false, false)
                                 : mgr.call(QStringLiteral("DisableUnitFiles"), units, false);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error)
            *error = reply.errorMessage();
        return false;
    }
    reply = mgr.call(QStringLiteral("Reload"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error)
            *error = QStringLiteral("daemon-reload failed: ") + reply.errorMessage();
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------
// logind
// ------------------------------------------------------------------------------------------
std::optional<QVariantMap> LinuxSystemOps::sessionProperties(const QString &sessionId)
{
    QDBusInterface mgr(kLogind, kLogindPath, kLogindManager, m_bus);
    const QDBusReply<QDBusObjectPath> path = mgr.call(QStringLiteral("GetSession"), sessionId);
    if (!path.isValid())
        return std::nullopt;
    QDBusInterface props(kLogind, path.value().path(), QStringLiteral("org.freedesktop.DBus.Properties"), m_bus);
    const QDBusReply<QVariantMap> all = props.call(QStringLiteral("GetAll"), QStringLiteral("org.freedesktop.login1.Session"));
    if (!all.isValid())
        return std::nullopt;
    return all.value();
}

bool LinuxSystemOps::terminateSession(const QString &sessionId, QString *error)
{
    QDBusInterface mgr(kLogind, kLogindPath, kLogindManager, m_bus);
    const QDBusMessage reply = mgr.call(QStringLiteral("TerminateSession"), sessionId);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error)
            *error = reply.errorMessage();
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------
// Client addresses: xrdp_display_N (server end, held by Xorg) --peer--> xrdp child --TCP--> client
// ------------------------------------------------------------------------------------------
namespace {

// The kernel's NLMSG_*/RTA_* macros use C casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wsign-compare"

// inode -> peer inode for connected AF_UNIX sockets, via NETLINK_SOCK_DIAG (UNIX_DIAG_PEER).
std::optional<QHash<quint64, quint64>> unixPeers(QString *error)
{
    const int fd = ::socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_SOCK_DIAG);
    if (fd < 0) {
        if (error)
            *error = QStringLiteral("sock_diag socket: ") + errnoText();
        return std::nullopt;
    }
    struct {
        nlmsghdr nlh;
        unix_diag_req req;
    } request{};
    request.nlh.nlmsg_len = sizeof(request);
    request.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    request.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.req.sdiag_family = AF_UNIX;
    request.req.udiag_states = 1u << 1; // TCP_ESTABLISHED == connected unix sockets
    request.req.udiag_show = UDIAG_SHOW_PEER;
    if (::send(fd, &request, sizeof(request), 0) < 0) {
        if (error)
            *error = QStringLiteral("sock_diag send: ") + errnoText();
        ::close(fd);
        return std::nullopt;
    }
    QHash<quint64, quint64> peers;
    alignas(nlmsghdr) char buf[32768];
    bool done = false;
    while (!done) {
        const ssize_t len = ::recv(fd, buf, sizeof(buf), 0);
        if (len < 0) {
            if (errno == EINTR)
                continue;
            if (error)
                *error = QStringLiteral("sock_diag recv: ") + errnoText();
            ::close(fd);
            return std::nullopt;
        }
        int remaining = int(len);
        for (auto *h = reinterpret_cast<nlmsghdr *>(buf); NLMSG_OK(h, remaining); h = NLMSG_NEXT(h, remaining)) {
            if (h->nlmsg_type == NLMSG_DONE) {
                done = true;
                break;
            }
            if (h->nlmsg_type == NLMSG_ERROR) {
                if (error)
                    *error = QStringLiteral("sock_diag returned an error");
                ::close(fd);
                return std::nullopt;
            }
            auto *msg = static_cast<unix_diag_msg *>(NLMSG_DATA(h));
            int attrLen = int(h->nlmsg_len - NLMSG_LENGTH(sizeof(*msg)));
            for (auto *a = reinterpret_cast<rtattr *>(msg + 1); RTA_OK(a, attrLen); a = RTA_NEXT(a, attrLen)) {
                if (a->rta_type == UNIX_DIAG_PEER && RTA_PAYLOAD(a) >= sizeof(quint32))
                    peers.insert(msg->udiag_ino, *static_cast<quint32 *>(RTA_DATA(a)));
            }
        }
    }
    ::close(fd);
    return peers;
}
#pragma GCC diagnostic pop

QList<quint64> socketInodesOf(int pid)
{
    QList<quint64> inodes;
    static const QRegularExpression re(QStringLiteral(R"(^socket:\[(\d+)\]$)"));
    const QDir fdDir(QStringLiteral("/proc/%1/fd").arg(pid));
    for (const QString &fd : fdDir.entryList(QDir::System | QDir::NoDotAndDotDot)) {
        // raw link text is "socket:[N]"; read it without resolving
        const QByteArray link = QFile::encodeName(fdDir.filePath(fd));
        char target[64] = {};
        const ssize_t n = ::readlink(link.constData(), target, sizeof(target) - 1);
        if (n <= 0)
            continue;
        const auto m = re.match(QString::fromLatin1(target, int(n)));
        if (m.hasMatch())
            inodes << m.captured(1).toULongLong();
    }
    return inodes;
}

QByteArray readProc(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

std::optional<QVariantMap> LinuxSystemOps::xrdpClientAddresses(QString *error)
{
    // 1. server ends of the per-display sockets (only present while a client is attached)
    QHash<quint64, int> displayByServerInode;
    static const QRegularExpression dispRe(QStringLiteral(R"(/xrdp_display_(\d+)$)"));
    for (const auto &s : proc::parseProcNetUnix(readProc(QStringLiteral("/proc/net/unix")))) {
        if (s.state != proc::kUnixConnected)
            continue;
        const auto m = dispRe.match(s.path);
        if (m.hasMatch())
            displayByServerInode.insert(s.inode, m.captured(1).toInt());
    }
    QVariantMap result;
    if (displayByServerInode.isEmpty())
        return result;

    const auto peers = unixPeers(error);
    if (!peers)
        return std::nullopt;
    QHash<quint64, int> displayByClientInode;
    for (auto it = displayByServerInode.cbegin(); it != displayByServerInode.cend(); ++it)
        if (const quint64 peer = peers->value(it.key()); peer)
            displayByClientInode.insert(peer, it.value());

    // 2. established TCP connections by inode
    QHash<quint64, proc::TcpSocket> tcpByInode;
    for (const auto &t : proc::parseProcNetTcp(readProc(QStringLiteral("/proc/net/tcp")), false))
        if (t.state == proc::kTcpEstablished)
            tcpByInode.insert(t.inode, t);
    for (const auto &t : proc::parseProcNetTcp(readProc(QStringLiteral("/proc/net/tcp6")), true))
        if (t.state == proc::kTcpEstablished)
            tcpByInode.insert(t.inode, t);

    // 3. xrdp processes: the one holding the display's client socket also holds the RDP TCP socket
    const QDir procDir(QStringLiteral("/proc"));
    for (const QString &entry : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const int pid = entry.toInt(&isPid);
        if (!isPid)
            continue;
        if (readProc(QStringLiteral("/proc/%1/comm").arg(pid)).trimmed() != "xrdp")
            continue;
        const QList<quint64> inodes = socketInodesOf(pid);
        int display = -1;
        for (quint64 ino : inodes)
            if (displayByClientInode.contains(ino))
                display = displayByClientInode.value(ino);
        if (display < 0)
            continue;
        for (quint64 ino : inodes) {
            const auto it = tcpByInode.constFind(ino);
            if (it == tcpByInode.constEnd())
                continue;
            const QString addr = it->ipv6 && it->remoteAddress.contains(QLatin1Char(':'))
                                     ? QStringLiteral("[%1]:%2").arg(it->remoteAddress).arg(it->remotePort)
                                     : QStringLiteral("%1:%2").arg(it->remoteAddress).arg(it->remotePort);
            result.insert(QString::number(display), addr);
        }
    }
    return result;
}

} // namespace urdm::helper
