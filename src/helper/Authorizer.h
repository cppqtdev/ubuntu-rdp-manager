#pragma once

#include <QString>
#include <functional>

namespace urdm::helper {

struct AuthResult
{
    bool authorized = false;
    bool challenge = false;   // polkit wanted to ask but interaction was not allowed
    QString error;            // set when polkit itself could not be queried
};

class Authorizer
{
public:
    virtual ~Authorizer() = default;
    // busName: unique D-Bus name of the caller (":1.42"). Must never be taken from message
    // payload; the helper passes QDBusMessage::service().
    virtual void check(const QString &busName, const QString &actionId, bool allowInteraction,
                       std::function<void(AuthResult)> done) = 0;
};

} // namespace urdm::helper
