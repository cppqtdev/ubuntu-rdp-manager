#pragma once

#include <QString>
#include <optional>

namespace urdm::validators {

// POSIX-portable Linux account/group names as accepted by useradd/groupadd defaults.
bool isValidUserName(const QString &name);
bool isValidGroupName(const QString &name);

// logind session identifiers ("c1", "42", ...).
bool isValidLogindSessionId(const QString &id);

// Parses an X11 display string ":10" or ":10.0" into the display number.
std::optional<int> parseDisplayNumber(const QString &display);

// Absolute, normalised path without "..", control characters or excessive length.
// Does not touch the filesystem.
bool isSafeAbsolutePath(const QString &path);

bool isValidTcpPort(int port);

// Values written into INI lines must not be able to inject new lines or sections.
bool isSafeIniValue(const QString &value);

// OpenSSL cipher-list syntax characters only.
bool isValidCipherList(const QString &value);

// DNS name or IP literal usable as a certificate common name / connection target.
bool isValidHostName(const QString &host);

} // namespace urdm::validators
