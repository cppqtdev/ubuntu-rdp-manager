#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>

namespace urdm {

// Line-preserving INI document matching xrdp's file_read_section() semantics:
// section and key names compare case-insensitively, ';' and '#' start comment lines,
// duplicate keys are legal (e.g. repeated "param=" lines in [Xorg]).
// Editing changes only the affected lines so comments, ordering and unrelated
// administrator settings survive a round trip byte-for-byte.
class IniDocument
{
public:
    static IniDocument parse(const QByteArray &data);

    QByteArray serialize() const;

    bool hasSection(const QString &section) const;
    QStringList sections() const;
    bool hasKey(const QString &section, const QString &key) const;
    QString value(const QString &section, const QString &key, const QString &defaultValue = {}) const;
    QStringList values(const QString &section, const QString &key) const;
    QStringList keys(const QString &section) const;

    // Replaces the first occurrence (keeping the original key spelling) or appends the key
    // at the end of the section; creates the section at the end of the file if needed.
    void setValue(const QString &section, const QString &key, const QString &value);
    // Removes every occurrence of the key in the section. Returns the number removed.
    int removeKey(const QString &section, const QString &key);

private:
    enum class LineType { Blank, Comment, Section, KeyValue, Other };
    struct Line {
        LineType type = LineType::Other;
        QString raw;      // original text without line terminator
        QString section;  // Section: name; KeyValue: owning section
        QString key;
        QString value;
    };

    int findKey(const QString &section, const QString &key) const;
    int sectionHeaderIndex(const QString &section) const;
    int sectionInsertIndex(const QString &section) const;

    QList<Line> m_lines;
    bool m_trailingNewline = true;
    QString m_newline = QStringLiteral("\n");
};

} // namespace urdm
