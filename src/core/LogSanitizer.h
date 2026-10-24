#pragma once

#include <QString>

namespace urdm {

// Removes secrets from log text before it is shown in exports/diagnostic bundles:
// password-like key/value pairs, base64-encoded xrdp credentials, PEM private keys,
// FreeRDP /p: arguments and bearer/auth tokens.
class LogSanitizer
{
public:
    static QString sanitize(const QString &text);
};

} // namespace urdm
