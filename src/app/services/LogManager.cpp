#include "LogManager.h"

#include "HelperClient.h"
#include "HelperProtocol.h"
#include "LogSanitizer.h"
#include "ProcessRunner.h"

#include <QDateTime>
#include <QSaveFile>
#include <QUrl>

namespace urdm::app {

namespace {
constexpr int kMaxRows = 5000;
constexpr int kInitialLines = 1500;
}

// ---------------- LogListModel ----------------
QString LogListModel::severity(int p)
{
    if (p <= 3)
        return QStringLiteral("error");
    if (p == 4)
        return QStringLiteral("warning");
    if (p <= 6)
        return QStringLiteral("info");
    return QStringLiteral("debug");
}

QVariant LogListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    // newest first
    const LogEntry &e = m_rows.at(m_rows.size() - 1 - index.row());
    switch (role) {
    case TimeRole: return e.timeMs;
    case TimeTextRole: return QDateTime::fromMSecsSinceEpoch(e.timeMs).toString(QStringLiteral("MM-dd HH:mm:ss"));
    case PriorityRole: return e.priority;
    case SeverityRole: return severity(e.priority);
    case SourceRole: return e.source;
    case MessageRole: return e.message;
    case CategoryRole: return e.category;
    case UserRole_: return e.user;
    case AddressRole: return e.address;
    case PlainRole: return e.plain;
    case PrimaryRole: return e.primary;
    default: return {};
    }
}

QHash<int, QByteArray> LogListModel::roleNames() const
{
    return {{TimeRole, "time"}, {TimeTextRole, "timeText"}, {PriorityRole, "priority"}, {SeverityRole, "severity"},
            {SourceRole, "source"}, {MessageRole, "message"}, {CategoryRole, "category"}, {UserRole_, "user"}, {AddressRole, "address"}, {PlainRole, "plain"}, {PrimaryRole, "primary"}};
}

void LogListModel::append(const QList<LogEntry> &entries, int maxRows)
{
    if (entries.isEmpty())
        return;
    beginResetModel();
    m_rows.append(entries);
    if (m_rows.size() > maxRows)
        m_rows.remove(0, m_rows.size() - maxRows);
    endResetModel();
}

void LogListModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

// ---------------- LogFilterModel ----------------
LogFilterModel::LogFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::modelReset, this, &LogFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &LogFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &LogFilterModel::countChanged);
}

void LogFilterModel::setSearchText(const QString &s)
{
    if (s == m_search)
        return;
    m_search = s;
    invalidateFilter();
    emit filterChanged();
    emit countChanged();
}

void LogFilterModel::setMaxPriority(int p)
{
    if (p == m_maxPriority)
        return;
    m_maxPriority = p;
    invalidateFilter();
    emit filterChanged();
    emit countChanged();
}

void LogFilterModel::setCategory(const QString &c)
{
    if (c == m_category)
        return;
    m_category = c;
    invalidateFilter();
    emit filterChanged();
    emit countChanged();
}

void LogFilterModel::setMode(const QString &m)
{
    if (m == m_mode)
        return;
    m_mode = m;
    invalidateFilter();
    emit filterChanged();
    emit countChanged();
}

bool LogFilterModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const QModelIndex idx = sourceModel()->index(row, 0, parent);
    if (m_mode != QLatin1String("all")) {
        static const QStringList problems{QStringLiteral("auth-failure"), QStringLiteral("access-denied"), QStringLiteral("limit-denied"),
                                          QStringLiteral("session-failure"), QStringLiteral("tls-problem")};
        const QString cat = idx.data(LogListModel::CategoryRole).toString();
        if (!idx.data(LogListModel::PrimaryRole).toBool())
            return false;
        if (m_mode == QLatin1String("problems") && !problems.contains(cat))
            return false;
        if (cat == QLatin1String("connection") || cat == QLatin1String("info") || cat == QLatin1String("error"))
            return false;
    }
    if (idx.data(LogListModel::PriorityRole).toInt() > m_maxPriority)
        return false;
    if (!m_category.isEmpty() && idx.data(LogListModel::CategoryRole).toString() != m_category)
        return false;
    if (!m_search.isEmpty() && !idx.data(LogListModel::MessageRole).toString().contains(m_search, Qt::CaseInsensitive)
        && !idx.data(LogListModel::PlainRole).toString().contains(m_search, Qt::CaseInsensitive)
        && !idx.data(LogListModel::SourceRole).toString().contains(m_search, Qt::CaseInsensitive))
        return false;
    return true;
}

// ---------------- LogManager ----------------
LogManager::LogManager(HelperClient *helper, QObject *parent)
    : QObject(parent)
    , m_helper(helper)
    , m_model(new LogListModel(this))
    , m_filter(new LogFilterModel(this))
{
    m_filter->setSourceModel(m_model);
    m_followTimer.setInterval(5000);
    connect(&m_followTimer, &QTimer::timeout, this, [this]() { fetch(true); });
    m_followTimer.start();
    reload();
}

const QList<LogEntry> &LogManager::entries() const
{
    return m_model->rows();
}

int LogManager::countSince(const QString &category, qint64 sinceMs) const
{
    int n = 0;
    for (const LogEntry &e : m_model->rows())
        n += e.timeMs >= sinceMs && e.category == category;
    return n;
}

void LogManager::setFollow(bool on)
{
    if (on == follow())
        return;
    on ? m_followTimer.start() : m_followTimer.stop();
    emit stateChanged();
}

void LogManager::reload()
{
    m_cursor.clear();
    m_viaHelper = false;
    fetch(false);
}

void LogManager::fetch(bool incremental)
{
    if (m_loading || (incremental && m_viaHelper))
        return;
    m_loading = true;
    emit stateChanged();
    QStringList args{QStringLiteral("--no-pager"), QStringLiteral("-o"), QStringLiteral("json"),
                     QStringLiteral("-u"), urdm::protocol::kXrdpUnit, QStringLiteral("-u"), urdm::protocol::kSesmanUnit};
    if (incremental && !m_cursor.isEmpty())
        args << QStringLiteral("--after-cursor") << m_cursor;
    else
        args << QStringLiteral("-n") << QString::number(kInitialLines);
    ProcessRunner::run(this, QStringLiteral("journalctl"), args, 30000, [this, incremental](const ProcessResult &r) {
        m_loading = false;
        if (r.startFailed) {
            m_lastError = QStringLiteral("journalctl is not available");
        } else {
            const QString err = QString::fromUtf8(r.stdErr);
            // journalctl exits 0 but hides system logs from users outside adm/systemd-journal
            m_needsElevation = err.contains(QLatin1String("insufficient permissions"))
                               || err.contains(QLatin1String("not seeing messages from other users"));
            m_lastError = r.ok() ? QString() : err.trimmed().left(300);
            ingest(r.stdOut, !incremental);
        }
        emit stateChanged();
    });
}

void LogManager::loadViaHelper()
{
    m_loading = true;
    emit stateChanged();
    m_helper->call(QStringLiteral("ReadJournal"), {kInitialLines, qlonglong(0)}, [this](const HelperReply &r) {
        m_loading = false;
        if (r.ok) {
            m_viaHelper = true;
            m_needsElevation = false;
            m_lastError.clear();
            ingest(r.value.toString().toUtf8(), true);
        } else {
            m_lastError = r.message;
            emit notification(r.notAuthorized ? QStringLiteral("warning") : QStringLiteral("error"), r.message);
        }
        emit stateChanged();
    });
}

void LogManager::ingest(const QByteArray &jsonLines, bool replace)
{
    QList<LogEntry> entries;
    for (const QByteArray &line : jsonLines.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        if (auto e = LogClassifier::parseJournalJson(line)) {
            entries << *e;
            m_cursor = e->cursor;
        }
    }
    if (replace)
        m_model->clear();
    m_model->append(entries, kMaxRows);
    if (replace || !entries.isEmpty()) {
        recount();
        emit stateChanged();
    }
}

void LogManager::recount()
{
    const qint64 since = QDateTime::currentMSecsSinceEpoch() - 24LL * 3600 * 1000;
    m_counts.clear();
    m_recentProblems.clear();
    m_lastDenied.clear();
    const auto &rows = m_model->rows();
    for (const LogEntry &e : rows) {
        if (e.timeMs >= since && e.primary)
            m_counts[e.category] += 1;
        if ((e.category == QLatin1String("access-denied") || e.category == QLatin1String("limit-denied")
             || e.category == QLatin1String("auth-failure")) && !e.user.isEmpty())
            m_lastDenied.insert(e.user, QStringLiteral("%1: %2").arg(QDateTime::fromMSecsSinceEpoch(e.timeMs).toString(QStringLiteral("MM-dd HH:mm")),
                                                                      e.category));
    }
    static const QStringList problemCats{QStringLiteral("auth-failure"), QStringLiteral("access-denied"), QStringLiteral("limit-denied"),
                                         QStringLiteral("session-failure"), QStringLiteral("tls-problem")};
    for (qsizetype i = rows.size() - 1; i >= 0 && m_recentProblems.size() < 8; --i) {
        const LogEntry &e = rows.at(i);
        if (!problemCats.contains(e.category) || !e.primary || e.timeMs < since)
            continue;
        m_recentProblems << QVariantMap{{QStringLiteral("time"), QDateTime::fromMSecsSinceEpoch(e.timeMs).toString(QStringLiteral("dd MMM, HH:mm"))},
                                        {QStringLiteral("category"), e.category},
                                        {QStringLiteral("message"), LogSanitizer::sanitize(e.plain)}};
    }
    emit countersChanged();
}

QString LogManager::sanitizedText(int maxLines) const
{
    QStringList lines;
    const int n = m_filter->rowCount();
    for (int i = 0; i < n && i < maxLines; ++i) {
        const QModelIndex idx = m_filter->index(i, 0);
        lines << QStringLiteral("%1 [%2] %3: %4")
                     .arg(QDateTime::fromMSecsSinceEpoch(idx.data(LogListModel::TimeRole).toLongLong()).toString(Qt::ISODate),
                          idx.data(LogListModel::SeverityRole).toString(), idx.data(LogListModel::SourceRole).toString(),
                          idx.data(LogListModel::MessageRole).toString());
    }
    return LogSanitizer::sanitize(lines.join(QLatin1Char('\n')));
}

bool LogManager::exportFiltered(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    QSaveFile f(url.isLocalFile() ? url.toLocalFile() : fileUrl);
    if (!f.open(QIODevice::WriteOnly)) {
        emit notification(QStringLiteral("error"), QStringLiteral("Cannot write %1: %2").arg(f.fileName(), f.errorString()));
        return false;
    }
    f.write(sanitizedText(kMaxRows).toUtf8());
    f.write("\n");
    if (!f.commit()) {
        emit notification(QStringLiteral("error"), f.errorString());
        return false;
    }
    emit notification(QStringLiteral("success"), QStringLiteral("Exported %1 sanitized log lines").arg(m_filter->rowCount()));
    return true;
}

} // namespace urdm::app
