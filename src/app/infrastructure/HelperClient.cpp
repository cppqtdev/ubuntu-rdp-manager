#include "HelperClient.h"

#include "HelperProtocol.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace urdm::app {

namespace p = urdm::protocol;

namespace {
// long enough for the administrator to read and answer the polkit password dialog
constexpr int kCallTimeoutMs = 6 * 60 * 1000;
}

HelperClient::HelperClient(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
    m_bus.connect(p::kServiceName, p::kObjectPath, p::kInterface, QStringLiteral("JobOutput"), this,
                  SLOT(onJobOutput(QString,QString)));
    m_bus.connect(p::kServiceName, p::kObjectPath, p::kInterface, QStringLiteral("JobFinished"), this,
                  SLOT(onJobFinished(QString,bool,int,QString)));
    refreshAvailability();
}

void HelperClient::refreshAvailability()
{
    // The helper is D-Bus activatable; it is "available" when installed, not only when running.
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                                      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ListActivatableNames"));
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 10000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
        w->deleteLater();
        const QDBusPendingReply<QStringList> r = *w;
        const bool avail = !r.isError() && r.value().contains(p::kServiceName);
        if (avail != m_available) {
            m_available = avail;
            emit availableChanged();
        }
    });
}

void HelperClient::call(const QString &method, const QVariantList &args, std::function<void(const HelperReply &)> done)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(p::kServiceName, p::kObjectPath, p::kInterface, method);
    msg.setArguments(args);
    msg.setInteractiveAuthorizationAllowed(true);
    ++m_pending;
    emit pendingCallsChanged();
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, kCallTimeoutMs), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, done, method]() {
        w->deleteLater();
        --m_pending;
        emit pendingCallsChanged();
        const QDBusMessage reply = w->reply();
        HelperReply r;
        if (reply.type() == QDBusMessage::ReplyMessage) {
            r.ok = true;
            r.value = reply.arguments().value(0);
            if (r.value.typeId() == QMetaType::QString)
                r.message = r.value.toString();
        } else {
            r.notAuthorized = reply.errorName() == p::kErrNotAuthorized;
            if (r.notAuthorized)
                r.message = QStringLiteral("Administrator permission was not given, so nothing was changed. "
                                           "Try again and enter your password when Ubuntu asks.");
            else if (reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown"))
                r.message = QStringLiteral("A part of this program is missing. Please reinstall Ubuntu RDP Manager.");
            else if (reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.NoReply"))
                r.message = QStringLiteral("This took too long and was stopped. Please try again.");
            else
                r.message = reply.errorMessage().isEmpty() ? reply.errorName() : reply.errorMessage();
        }
        done(r);
    });
}

void HelperClient::onJobOutput(const QString &jobId, const QString &line)
{
    emit jobOutput(jobId, line);
}

void HelperClient::onJobFinished(const QString &jobId, bool success, int exitCode, const QString &summary)
{
    emit jobFinished(jobId, success, exitCode, summary);
}

} // namespace urdm::app
