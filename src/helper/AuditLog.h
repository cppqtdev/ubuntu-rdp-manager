#pragma once

#include <QString>
#include <QStringList>

namespace urdm::helper {

// Append-only audit trail of administrative actions. Each record goes to the journal
// (helper stderr, SYSLOG_IDENTIFIER=urdm-helper) and to a root:adm 0640 file.
class AuditLog
{
public:
    explicit AuditLog(QString filePath);
    void record(unsigned uid, const QString &user, const QString &operation, const QStringList &arguments,
                const QString &outcome, const QString &detail);

private:
    QString m_path;
};

} // namespace urdm::helper
