#include "SessionLimitPolicy.h"


namespace urdm {

SessionLimitPolicy::SessionLimitPolicy(XrdpVersion version)
    : m_version(version)
{
}

bool SessionLimitPolicy::zeroMeansUnlimited() const
{
    // Unknown version: assume the conservative (0.9.x) semantics so we never write 0.
    return m_version.valid && m_version >= XrdpVersion{0, 10, 0, true};
}

int SessionLimitPolicy::unlimitedConfigValue() const
{
    return zeroMeansUnlimited() ? 0 : kLegacyUnlimitedSentinel;
}

SessionLimitPolicy::State SessionLimitPolicy::interpret(const QString &rawValue) const
{
    State s;
    const QString trimmed = rawValue.trimmed();
    if (trimmed.isEmpty()) {
        // sesman's built-in default for an absent key is 0 in both 0.9 and 0.10.
        s.configuredValue = 0;
    } else {
        bool ok = false;
        s.configuredValue = trimmed.toInt(&ok);
        if (!ok || s.configuredValue < 0) {
            s.kind = Kind::Invalid;
            s.description = QStringLiteral("MaxSessions value \"%1\" is not a non-negative integer").arg(trimmed);
            return s;
        }
    }

    if (s.configuredValue == 0) {
        if (zeroMeansUnlimited()) {
            s.kind = Kind::Unlimited;
            s.description = QStringLiteral("Unlimited (configuration) – actual capacity depends on hardware");
        } else {
            s.kind = Kind::DeniesAllLogins;
            s.description = QStringLiteral("MaxSessions=0 makes xrdp %1 refuse every login").arg(m_version.toString());
        }
        return s;
    }
    if (!zeroMeansUnlimited() && s.configuredValue >= kLegacyUnlimitedSentinel) {
        s.kind = Kind::Unlimited;
        s.description = QStringLiteral("Unlimited (configuration, MaxSessions=%1) – actual capacity depends on hardware")
                            .arg(s.configuredValue);
        return s;
    }
    s.kind = Kind::Limited;
    s.limit = s.configuredValue;
    s.description = QStringLiteral("At most %1 concurrent sessions").arg(s.limit);
    return s;
}

std::optional<int> SessionLimitPolicy::toConfigValue(bool unlimited, int maxSessions, QString *error) const
{
    if (unlimited)
        return unlimitedConfigValue();
    if (maxSessions < 1 || maxSessions > kMaxExplicitLimit) {
        if (error)
            *error = QStringLiteral("Maximum sessions must be between 1 and %1").arg(kMaxExplicitLimit);
        return std::nullopt;
    }
    return maxSessions;
}

} // namespace urdm
