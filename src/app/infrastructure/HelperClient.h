#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QVariant>
#include <functional>

namespace urdm::app {

struct HelperReply
{
    bool ok = false;
    bool notAuthorized = false;
    QString message;  // success message or error text (never contains secrets)
    QVariant value;   // first reply argument
};

// Client side of com.urdm.Helper1 (PrivilegedOperationClient). All calls are asynchronous and
// allow interactive polkit authorization, so the desktop's polkit agent shows the password
// dialog while the GUI stays responsive.
class HelperClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(int pendingCalls READ pendingCalls NOTIFY pendingCallsChanged)

public:
    explicit HelperClient(QDBusConnection bus, QObject *parent = nullptr);

    bool available() const { return m_available; }
    int pendingCalls() const { return m_pending; }
    void refreshAvailability();

    void call(const QString &method, const QVariantList &args, std::function<void(const HelperReply &)> done);

signals:
    void availableChanged();
    void pendingCallsChanged();
    void jobOutput(const QString &jobId, const QString &line);
    void jobFinished(const QString &jobId, bool success, int exitCode, const QString &summary);

private slots:
    void onJobOutput(const QString &jobId, const QString &line);
    void onJobFinished(const QString &jobId, bool success, int exitCode, const QString &summary);

private:
    QDBusConnection m_bus;
    bool m_available = false;
    int m_pending = 0;
};

} // namespace urdm::app
