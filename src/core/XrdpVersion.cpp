#include "XrdpVersion.h"

#include <QRegularExpression>

namespace urdm {

XrdpVersion XrdpVersion::fromDebianVersion(const QString &packageVersion)
{
    // [epoch:]upstream[-debian_revision]; upstream begins with major.minor[.patch]
    static const QRegularExpression re(QStringLiteral(R"(^(?:\d+:)?(\d+)\.(\d+)(?:\.(\d+))?)"));
    XrdpVersion v;
    const auto m = re.match(packageVersion.trimmed());
    if (!m.hasMatch())
        return v;
    v.major = m.captured(1).toInt();
    v.minor = m.captured(2).toInt();
    v.patch = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
    v.valid = true;
    return v;
}

QString XrdpVersion::toString() const
{
    if (!valid)
        return QStringLiteral("unknown");
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

} // namespace urdm
