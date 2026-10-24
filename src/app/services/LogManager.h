#pragma once

#include "LogClassifier.h"

#include <QAbstractListModel>
#include <QObject>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVariantMap>

namespace urdm::app {

class HelperClient;

class LogListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { TimeRole = Qt::UserRole + 1, TimeTextRole, PriorityRole, SeverityRole, SourceRole, MessageRole, CategoryRole, UserRole_, AddressRole, PlainRole, PrimaryRole };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(m_rows.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void append(const QList<LogEntry> &entries, int maxRows);
    void clear();
    const QList<LogEntry> &rows() const { return m_rows; }
    static QString severity(int priority);
private:
    QList<LogEntry> m_rows;
};

class LogFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    Q_PROPERTY(int maxPriority READ maxPriority WRITE setMaxPriority NOTIFY filterChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY filterChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY filterChanged) // important | problems | all
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit LogFilterModel(QObject *parent = nullptr);
    QString searchText() const { return m_search; }
    void setSearchText(const QString &s);
    int maxPriority() const { return m_maxPriority; }
    void setMaxPriority(int p);
    QString category() const { return m_category; }
    void setCategory(const QString &c);
    QString mode() const { return m_mode; }
    void setMode(const QString &m);
    int count() const { return rowCount(); }
signals:
    void filterChanged();
    void countChanged();
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    QString m_search;
    int m_maxPriority = 7;
    QString m_category;
    QString m_mode = QStringLiteral("important");
};

// Reads the xrdp / xrdp-sesman journal. Members of "adm" or "systemd-journal" (Ubuntu's first
// user is in "adm") read it directly; otherwise the helper's ReadJournal is used after polkit
// authorization.
class LogManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(LogFilterModel *model READ model CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool follow READ follow WRITE setFollow NOTIFY stateChanged)
    Q_PROPERTY(bool needsElevation READ needsElevation NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY stateChanged)
    Q_PROPERTY(int authFailures24h READ authFailures24h NOTIFY countersChanged)
    Q_PROPERTY(int accessDenied24h READ accessDenied24h NOTIFY countersChanged)
    Q_PROPERTY(int limitDenied24h READ limitDenied24h NOTIFY countersChanged)
    Q_PROPERTY(int sessionFailures24h READ sessionFailures24h NOTIFY countersChanged)
    Q_PROPERTY(int sessionStarts24h READ sessionStarts24h NOTIFY countersChanged)
    Q_PROPERTY(int tlsProblems24h READ tlsProblems24h NOTIFY countersChanged)
    Q_PROPERTY(QVariantList recentProblems READ recentProblems NOTIFY countersChanged)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)

public:
    explicit LogManager(HelperClient *helper, QObject *parent = nullptr);

    LogFilterModel *model() const { return m_filter; }
    bool loading() const { return m_loading; }
    bool follow() const { return m_followTimer.isActive(); }
    void setFollow(bool on);
    bool needsElevation() const { return m_needsElevation; }
    QString lastError() const { return m_lastError; }
    int totalCount() const { return m_model->rowCount(); }
    int authFailures24h() const { return m_counts.value(QStringLiteral("auth-failure")); }
    int accessDenied24h() const { return m_counts.value(QStringLiteral("access-denied")); }
    int limitDenied24h() const { return m_counts.value(QStringLiteral("limit-denied")); }
    int sessionFailures24h() const { return m_counts.value(QStringLiteral("session-failure")); }
    int sessionStarts24h() const { return m_counts.value(QStringLiteral("session-start")); }
    int tlsProblems24h() const { return m_counts.value(QStringLiteral("tls-problem")); }
    QVariantList recentProblems() const { return m_recentProblems; }
    QStringList categories() const { return LogClassifier::categories(); }
    QVariantMap lastDeniedByUser() const { return m_lastDenied; }
    const QList<LogEntry> &entries() const;
    int countSince(const QString &category, qint64 sinceMs) const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadViaHelper();
    Q_INVOKABLE bool exportFiltered(const QString &fileUrl);
    Q_INVOKABLE QString sanitizedText(int maxLines) const;

signals:
    void stateChanged();
    void countersChanged();
    void notification(const QString &level, const QString &text);

private:
    void fetch(bool incremental);
    void ingest(const QByteArray &jsonLines, bool replace);
    void recount();

    HelperClient *m_helper;
    LogListModel *m_model;
    LogFilterModel *m_filter;
    QTimer m_followTimer;
    QString m_cursor;
    bool m_loading = false;
    bool m_needsElevation = false;
    bool m_viaHelper = false;
    QString m_lastError;
    QHash<QString, int> m_counts;
    QVariantList m_recentProblems;
    QVariantMap m_lastDenied;
};

} // namespace urdm::app
