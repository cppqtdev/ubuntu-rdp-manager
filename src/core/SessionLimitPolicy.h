#pragma once

#include "XrdpVersion.h"

#include <QString>
#include <optional>

namespace urdm {

// Translates between the administrator's intent ("unlimited" / "at most N sessions")
// and the sesman.ini [Sessions] MaxSessions value for the installed xrdp version.
//
// Verified behaviour (docs/compatibility.md):
//  * xrdp < 0.10: MaxSessions=0 makes sesman refuse EVERY login (session.c compares
//    count >= max_sessions), despite the man page. "Unlimited" is written as a sentinel.
//  * xrdp >= 0.10: MaxSessions=0 means no limit.
class SessionLimitPolicy
{
public:
    enum class Kind { Unlimited, Limited, DeniesAllLogins, Invalid };

    struct State {
        Kind kind = Kind::Invalid;
        int configuredValue = 0;  // raw MaxSessions value
        int limit = 0;            // effective limit when kind == Limited
        QString description;      // human readable, never promises hardware capacity
    };

    static constexpr int kLegacyUnlimitedSentinel = 9999;
    static constexpr int kMaxExplicitLimit = 9998;

    explicit SessionLimitPolicy(XrdpVersion version);

    bool zeroMeansUnlimited() const;
    int unlimitedConfigValue() const;

    // Interprets the raw MaxSessions string (may be empty when the key is absent).
    State interpret(const QString &rawValue) const;

    // Returns the MaxSessions value to write, or an error message.
    std::optional<int> toConfigValue(bool unlimited, int maxSessions, QString *error = nullptr) const;

private:
    XrdpVersion m_version;
};

} // namespace urdm
