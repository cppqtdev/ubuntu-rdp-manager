#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace urdm {

// One journald record of xrdp / xrdp-sesman, classified using message formats verified on
// xrdp 0.9.24 (tests/fixtures/journal-xrdp-0.9.24.jsonl). Classification is a convenience for
// filtering and counters; it is never used as an authoritative session source.
struct LogEntry
{
    qint64 timeMs = 0;
    int priority = 6;          // syslog priority 0..7
    QString source;            // SYSLOG_IDENTIFIER or unit
    QString message;
    QString category;          // see LogClassifier::categories()
    QString user;              // when the message names a user
    QString address;           // when the message names a client address
    QString cursor;
    bool primary = true;       // false for extra lines xrdp writes about the same event
    QString plain;             // one plain-language sentence describing the event
};

class LogClassifier
{
public:
    static std::optional<LogEntry> parseJournalJson(const QByteArray &line);
    static void classify(LogEntry &e);
    static QString plainSentence(const LogEntry &e);
    static QStringList categories();
};

} // namespace urdm
