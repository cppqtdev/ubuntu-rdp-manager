#pragma once

#include <QList>
#include <QObject>
#include <QProcess>
#include <QStringList>

namespace urdm::helper {

// Runs an apt-get plan produced by HelperCore::buildInstallPlan sequentially and streams output.
// There is deliberately no cancel: interrupting dpkg can leave the package database broken.
class PackageJob : public QObject
{
    Q_OBJECT
public:
    PackageJob(QString id, QList<QStringList> plan, QObject *parent = nullptr);
    QString id() const { return m_id; }
    void start();

signals:
    void output(const QString &id, const QString &line);
    void finished(const QString &id, bool success, int exitCode, const QString &summary);

private:
    void runNext();
    void flush(bool final);

    QString m_id;
    QList<QStringList> m_plan;
    int m_step = 0;
    QProcess m_proc;
    QByteArray m_buffer;
};

} // namespace urdm::helper
