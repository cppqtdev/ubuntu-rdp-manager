#include "PolkitAuthorizer.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>

namespace urdm::helper {

namespace {
using StringMap = QMap<QString, QString>;
constexpr uint kAllowUserInteraction = 0x1;
// Interactive authentication waits for the administrator to type a password.
constexpr int kPolkitTimeoutMs = 5 * 60 * 1000;
} // namespace

PolkitAuthorizer::PolkitAuthorizer(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
{
    qDBusRegisterMetaType<StringMap>();
}

void PolkitAuthorizer::check(const QString &busName, const QString &actionId, bool allowInteraction,
                             std::function<void(AuthResult)> done)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PolicyKit1"),
                                                       QStringLiteral("/org/freedesktop/PolicyKit1/Authority"),
                                                       QStringLiteral("org.freedesktop.PolicyKit1.Authority"),
                                                       QStringLiteral("CheckAuthorization"));
    // subject: (sa{sv}) = ("system-bus-name", {"name": <busName>})
    QDBusArgument subject;
    subject.beginStructure();
    subject << QStringLiteral("system-bus-name");
    subject.beginMap(QMetaType::fromType<QString>(), QMetaType::fromType<QDBusVariant>());
    subject.beginMapEntry();
    subject << QStringLiteral("name") << QDBusVariant(busName);
    subject.endMapEntry();
    subject.endMap();
    subject.endStructure();

    call << QVariant::fromValue(subject) << actionId << QVariant::fromValue(StringMap{})
         << uint(allowInteraction ? kAllowUserInteraction : 0) << QString();

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, kPolkitTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, done]() {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        AuthResult r;
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            r.error = reply.errorMessage().isEmpty() ? QStringLiteral("polkit did not answer") : reply.errorMessage();
            done(r);
            return;
        }
        // result: (bba{ss}) = (is_authorized, is_challenge, details)
        const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
        StringMap details;
        arg.beginStructure();
        arg >> r.authorized >> r.challenge >> details;
        arg.endStructure();
        done(r);
    });
}

} // namespace urdm::helper
