#include "IniDocument.h"

namespace urdm {

namespace {
bool sameName(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}
} // namespace

IniDocument IniDocument::parse(const QByteArray &data)
{
    IniDocument doc;
    const QString text = QString::fromUtf8(data);
    if (text.contains(QLatin1String("\r\n")))
        doc.m_newline = QStringLiteral("\r\n");
    doc.m_trailingNewline = text.isEmpty() || text.endsWith(QLatin1Char('\n'));

    QStringList rawLines = text.split(QLatin1Char('\n'));
    if (doc.m_trailingNewline && !rawLines.isEmpty())
        rawLines.removeLast();

    QString currentSection;
    for (QString raw : std::as_const(rawLines)) {
        if (raw.endsWith(QLatin1Char('\r')))
            raw.chop(1);
        Line line;
        line.raw = raw;
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) {
            line.type = LineType::Blank;
        } else if (trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('#'))) {
            line.type = LineType::Comment;
        } else if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            line.type = LineType::Section;
            currentSection = trimmed.mid(1, trimmed.size() - 2).trimmed();
            line.section = currentSection;
        } else if (const qsizetype eq = raw.indexOf(QLatin1Char('=')); eq > 0) {
            line.type = LineType::KeyValue;
            line.section = currentSection;
            line.key = raw.left(eq).trimmed();
            line.value = raw.mid(eq + 1).trimmed();
        } else {
            line.type = LineType::Other;
            line.section = currentSection;
        }
        doc.m_lines.append(line);
    }
    return doc;
}

QByteArray IniDocument::serialize() const
{
    QString out;
    for (qsizetype i = 0; i < m_lines.size(); ++i) {
        out += m_lines.at(i).raw;
        if (i + 1 < m_lines.size() || m_trailingNewline)
            out += m_newline;
    }
    return out.toUtf8();
}

bool IniDocument::hasSection(const QString &section) const
{
    return sectionHeaderIndex(section) >= 0;
}

QStringList IniDocument::sections() const
{
    QStringList result;
    for (const Line &l : m_lines)
        if (l.type == LineType::Section)
            result << l.section;
    return result;
}

bool IniDocument::hasKey(const QString &section, const QString &key) const
{
    return findKey(section, key) >= 0;
}

QString IniDocument::value(const QString &section, const QString &key, const QString &defaultValue) const
{
    const int idx = findKey(section, key);
    return idx >= 0 ? m_lines.at(idx).value : defaultValue;
}

QStringList IniDocument::values(const QString &section, const QString &key) const
{
    QStringList result;
    for (const Line &l : m_lines)
        if (l.type == LineType::KeyValue && sameName(l.section, section) && sameName(l.key, key))
            result << l.value;
    return result;
}

QStringList IniDocument::keys(const QString &section) const
{
    QStringList result;
    for (const Line &l : m_lines)
        if (l.type == LineType::KeyValue && sameName(l.section, section))
            result << l.key;
    return result;
}

void IniDocument::setValue(const QString &section, const QString &key, const QString &value)
{
    const int idx = findKey(section, key);
    if (idx >= 0) {
        Line &l = m_lines[idx];
        l.value = value;
        l.raw = l.key + QLatin1Char('=') + value;
        return;
    }

    Line kv;
    kv.type = LineType::KeyValue;
    kv.section = section;
    kv.key = key;
    kv.value = value;
    kv.raw = key + QLatin1Char('=') + value;

    if (sectionHeaderIndex(section) < 0) {
        if (!m_lines.isEmpty() && m_lines.last().type != LineType::Blank) {
            Line blank;
            blank.type = LineType::Blank;
            m_lines.append(blank);
        }
        Line header;
        header.type = LineType::Section;
        header.section = section;
        header.raw = QLatin1Char('[') + section + QLatin1Char(']');
        m_lines.append(header);
        m_lines.append(kv);
        m_trailingNewline = true;
        return;
    }
    m_lines.insert(sectionInsertIndex(section), kv);
}

int IniDocument::removeKey(const QString &section, const QString &key)
{
    int removed = 0;
    for (qsizetype i = m_lines.size() - 1; i >= 0; --i) {
        const Line &l = m_lines.at(i);
        if (l.type == LineType::KeyValue && sameName(l.section, section) && sameName(l.key, key)) {
            m_lines.removeAt(i);
            ++removed;
        }
    }
    return removed;
}

int IniDocument::findKey(const QString &section, const QString &key) const
{
    for (qsizetype i = 0; i < m_lines.size(); ++i) {
        const Line &l = m_lines.at(i);
        if (l.type == LineType::KeyValue && sameName(l.section, section) && sameName(l.key, key))
            return int(i);
    }
    return -1;
}

int IniDocument::sectionHeaderIndex(const QString &section) const
{
    for (qsizetype i = 0; i < m_lines.size(); ++i) {
        const Line &l = m_lines.at(i);
        if (l.type == LineType::Section && sameName(l.section, section))
            return int(i);
    }
    return -1;
}

// Insert after the last key/value (or other content) line of the section's first block,
// so new keys land next to existing ones instead of after trailing comments/blank lines.
int IniDocument::sectionInsertIndex(const QString &section) const
{
    const int header = sectionHeaderIndex(section);
    int insertAt = header + 1;
    for (qsizetype i = header + 1; i < m_lines.size(); ++i) {
        const Line &l = m_lines.at(i);
        if (l.type == LineType::Section)
            break;
        if (l.type == LineType::KeyValue || l.type == LineType::Other)
            insertAt = int(i) + 1;
    }
    return insertAt;
}

} // namespace urdm
