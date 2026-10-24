#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

namespace urdm::app {

class HelperClient;
class RdpConfigurationManager;
class SystemdClient;

// Detects the platform and installation state and installs the fixed component set through
// the helper (apt-get, never downloaded scripts).
class InstallationManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(QString osName READ osName CONSTANT)
    Q_PROPERTY(QString osId READ osId CONSTANT)
    Q_PROPERTY(QString osVersion READ osVersion CONSTANT)
    Q_PROPERTY(QString osSupport READ osSupport CONSTANT)          // "verified", "untested", "unsupported"
    Q_PROPERTY(QString architecture READ architecture CONSTANT)
    Q_PROPERTY(bool architectureSupported READ architectureSupported CONSTANT)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(bool userIsAdmin READ userIsAdmin CONSTANT)
    Q_PROPERTY(bool userCanReadJournal READ userCanReadJournal CONSTANT)
    Q_PROPERTY(bool helperAvailable READ helperAvailable NOTIFY stateChanged)
    Q_PROPERTY(QVariantList components READ components NOTIFY stateChanged)
    Q_PROPERTY(bool installing READ installing NOTIFY stateChanged)
    Q_PROPERTY(QStringList installLog READ installLog NOTIFY logChanged)
    Q_PROPERTY(QString installResult READ installResult NOTIFY stateChanged)
    Q_PROPERTY(bool installSucceeded READ installSucceeded NOTIFY stateChanged)

public:
    InstallationManager(HelperClient *helper, RdpConfigurationManager *config, SystemdClient *systemd, QObject *parent = nullptr);

    QString osName() const { return m_osName; }
    QString osId() const { return m_osId; }
    QString osVersion() const { return m_osVersion; }
    QString osSupport() const;
    QString architecture() const;
    bool architectureSupported() const;
    QString userName() const;
    bool userIsAdmin() const;
    bool userCanReadJournal() const;
    bool helperAvailable() const;
    QVariantList components() const { return m_components; }
    bool installing() const { return !m_jobId.isEmpty(); }
    QStringList installLog() const { return m_log; }
    QString installResult() const { return m_result; }
    bool installSucceeded() const { return m_succeeded; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void install(const QStringList &components);

signals:
    void stateChanged();
    void logChanged();
    void notification(const QString &level, const QString &text);

private:
    void readOsRelease();
    void detectComponents();
    bool inGroup(const char *group) const;

    HelperClient *m_helper;
    RdpConfigurationManager *m_config;
    SystemdClient *m_systemd;
    QString m_osName, m_osId, m_osVersion;
    QVariantList m_components;
    QString m_jobId;
    QStringList m_log;
    QString m_result;
    bool m_succeeded = false;
};

} // namespace urdm::app
