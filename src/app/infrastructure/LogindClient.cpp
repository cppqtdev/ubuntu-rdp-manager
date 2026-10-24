#include "LogindClient.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace urdm::app {

namespace {
const QString kLogind = QStringLiteral("org.freedesktop.login1");
const QString kPath = QStringLiteral("/org/freedesktop/login1");
const QString kManager = QStringLiteral("org.freedesktop.login1.Manager");

struct Entry { QString id; uint uid; QString user; QString seat; QString path; };

// ListSessions returns a(susso)
QList<Entry> parseList(const QDBusArgument &arg)
{
    QList<Entry> out;
    arg.beginArray();
    while (!arg.atEnd()) {
        Entry e;
        QDBusObjectPath path;
        arg.beginStructure();
        arg >> e.id >> e.uid >> e.user >> e.seat >> path;
        arg.endStructure();
        e.path = path.path();
        out << e;
    }
    arg.endArray();
    return out;
}
} // namespace

LogindClient::LogindClient(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &LogindClient::refresh);
    m_bus.connect(kLogind, kPath, kManager, QStringLiteral("SessionNew"), this, SLOT(scheduleRefresh()));
    m_bus.connect(kLogind, kPath, kManager, QStringLiteral("SessionRemoved"), this, SLOT(scheduleRefresh()));
}

void LogindClient::scheduleRefresh()
{
    m_debounce.start();
}

void LogindClient::refresh()
{
    if (m_inFlight) {
        m_again = true;
        return;
    }
    m_inFlight = true;
    auto *w = new QDBusPendingCallWatcher(
        m_bus.asyncCall(QDBusMessage::createMethodCall(kLogind, kPath, kManager, QStringLiteral("ListSessions")), 10000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            m_inFlight = false;
            m_failed = true;
            emit sessionsChanged();
            return;
        }
        const QList<Entry> entries = parseList(reply.arguments().constFirst().value<QDBusArgument>());
        auto collected = std::make_shared<QList<LogindSession>>();
        auto remaining = std::make_shared<int>(int(entries.size()));
        auto finish = [this, collected]() {
            m_sessions = *collected;
            m_failed = false;
            m_inFlight = false;
            emit sessionsChanged();
            if (m_again) {
                m_again = false;
                refresh();
            }
        };
        if (entries.isEmpty()) {
            finish();
            return;
        }
        for (const Entry &e : entries) {
            QDBusMessage msg = QDBusMessage::createMethodCall(kLogind, e.path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                              QStringLiteral("GetAll"));
            msg << QStringLiteral("org.freedesktop.login1.Session");
            auto *pw = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 10000), this);
            connect(pw, &QDBusPendingCallWatcher::finished, this, [pw, e, collected, remaining, finish]() {
                pw->deleteLater();
                const QDBusPendingReply<QVariantMap> pr = *pw;
                if (!pr.isError()) {
                    const QVariantMap m = pr.value();
                    LogindSession s;
                    s.id = e.id;
                    s.uid = e.uid;
                    s.user = e.user;
                    s.objectPath = e.path;
                    s.service = m.value(QStringLiteral("Service")).toString();
                    s.display = m.value(QStringLiteral("Display")).toString();
                    s.type = m.value(QStringLiteral("Type")).toString();
                    s.state = m.value(QStringLiteral("State")).toString();
                    s.scope = m.value(QStringLiteral("Scope")).toString();
                    s.timestampUsec = m.value(QStringLiteral("Timestamp")).toULongLong();
                    s.leader = m.value(QStringLiteral("Leader")).toUInt();
                    s.remote = m.value(QStringLiteral("Remote")).toBool();
                    s.remoteHost = m.value(QStringLiteral("RemoteHost")).toString();
                    collected->append(s);
                }
                if (--*remaining == 0)
                    finish();
            });
        }
    });
}

} // namespace urdm::app
