#pragma once

#include <QString>
#include <compare>

namespace urdm {

// Upstream xrdp version extracted from a Debian package version such as "0.9.24-4ubuntu0.1".
struct XrdpVersion
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool valid = false;

    static XrdpVersion fromDebianVersion(const QString &packageVersion);
    QString toString() const;

    auto operator<=>(const XrdpVersion &o) const
    {
        if (auto c = major <=> o.major; c != 0) return c;
        if (auto c = minor <=> o.minor; c != 0) return c;
        return patch <=> o.patch;
    }
    bool operator==(const XrdpVersion &o) const = default;
};

} // namespace urdm
