#pragma once

#include "ConfigSchema.h"

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>

namespace urdm::app {

class RdpConfigurationManager;

// One row per ConfigSchema setting with its current (on-disk) and pending (edited) value.
class SettingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")

public:
    enum Roles {
        IdRole = Qt::UserRole + 1, CategoryRole, LabelRole, HelpRole, TypeRole, ChoicesRole, MinimumRole, MaximumRole,
        ValueRole, PendingValueRole, ModifiedRole, IsDefaultRole, InternalOnlyRole, FileRole, KeyRole, ErrorRole, ChoiceLabelsRole
    };

    explicit SettingsModel(RdpConfigurationManager *config, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void refreshAll();
    void refreshRow(const QString &id);

private:
    RdpConfigurationManager *m_config;
};

} // namespace urdm::app
