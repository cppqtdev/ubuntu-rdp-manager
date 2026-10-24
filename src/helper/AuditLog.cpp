#include "AuditLog.h"

#include "LogSanitizer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstdio>
#include <fcntl.h>
#include <grp.h>
#include <unistd.h>

namespace urdm::helper {

AuditLog::AuditLog(QString filePath)
    : m_path(std::move(filePath))
{
}

void AuditLog::record(unsigned uid, const QString &user, const QString &operation, const QStringList &arguments,
                      const QString &outcome, const QString &detail)
{
    QStringList args;
    for (QString a : arguments) {
        a.replace(QLatin1Char('\n'), QLatin1Char(' '));
        args << a.left(200);
    }
    QString d = detail;
    d.replace(QLatin1Char('\n'), QLatin1Char(' '));
    const QString line = LogSanitizer::sanitize(
        QStringLiteral("%1 uid=%2 user=%3 op=%4 args=[%5] outcome=%6 detail=\"%7\"")
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
            .arg(uid)
            .arg(user, operation, args.join(QStringLiteral(", ")), outcome, d.left(500)));

    std::fprintf(stderr, "AUDIT %s\n", line.toUtf8().constData());
    std::fflush(stderr);

    if (m_path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    const QByteArray path = QFile::encodeName(m_path);
    const int fd = ::open(path.constData(), O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0640);
    if (fd < 0)
        return;
    if (struct group *adm = ::getgrnam("adm")) {
        if (::fchown(fd, 0, adm->gr_gid) != 0)
            std::fprintf(stderr, "AUDIT warning: cannot set group of %s\n", path.constData());
    }
    const QByteArray data = (line + QLatin1Char('\n')).toUtf8();
    if (::write(fd, data.constData(), size_t(data.size())) != data.size())
        std::fprintf(stderr, "AUDIT warning: short write to %s\n", path.constData());
    ::close(fd);
}

} // namespace urdm::helper
