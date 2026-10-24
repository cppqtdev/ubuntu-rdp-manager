#include "SettingsModel.h"

#include "RdpConfigurationManager.h"

namespace urdm::app {

namespace {
QString typeName(SettingDef::Type t)
{
    using T = SettingDef::Type;
    switch (t) {
    case T::Bool: return QStringLiteral("bool");
    case T::Int: return QStringLiteral("int");
    case T::Enum: return QStringLiteral("enum");
    case T::EnumList: return QStringLiteral("enumlist");
    case T::String: return QStringLiteral("string");
    case T::Path: return QStringLiteral("path");
    case T::ListenSpec: return QStringLiteral("listen");
    case T::GroupName: return QStringLiteral("group");
    case T::CipherList: return QStringLiteral("ciphers");
    case T::TlsProtocols: return QStringLiteral("tlsprotocols");
    case T::MaxSessions: return QStringLiteral("maxsessions");
    }
    return QStringLiteral("string");
}
} // namespace

SettingsModel::SettingsModel(RdpConfigurationManager *config, QObject *parent)
    : QAbstractListModel(parent)
    , m_config(config)
{
}

int SettingsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(ConfigSchema::settings().size());
}

QVariant SettingsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const SettingDef &d = ConfigSchema::settings().at(index.row());
    switch (role) {
    case IdRole: return d.id;
    case CategoryRole: return d.category;
    case LabelRole: return d.label;
    case HelpRole: return d.help;
    case TypeRole: return typeName(d.type);
    case ChoicesRole: return d.choices;
    case MinimumRole: return d.minimum;
    case MaximumRole: return d.maximum;
    case ValueRole: return m_config->value(d.id);
    case PendingValueRole: return m_config->pendingValue(d.id);
    case ModifiedRole: return m_config->isModified(d.id);
    case IsDefaultRole: return m_config->isDefault(d.id);
    case InternalOnlyRole: return d.internalOnly;
    case FileRole: return configFileName(d.file);
    case KeyRole: return QStringLiteral("[%1] %2").arg(d.section, d.key);
    case ErrorRole: return m_config->pendingError(d.id);
    case ChoiceLabelsRole: return d.choiceLabels;
    default: return {};
    }
}

QHash<int, QByteArray> SettingsModel::roleNames() const
{
    return {{IdRole, "settingId"}, {CategoryRole, "category"}, {LabelRole, "label"}, {HelpRole, "help"},
            {TypeRole, "type"}, {ChoicesRole, "choices"}, {MinimumRole, "minimum"}, {MaximumRole, "maximum"},
            {ValueRole, "value"}, {PendingValueRole, "pendingValue"}, {ModifiedRole, "modified"},
            {IsDefaultRole, "isDefault"}, {InternalOnlyRole, "internalOnly"}, {FileRole, "file"}, {KeyRole, "iniKey"},
            {ErrorRole, "error"}, {ChoiceLabelsRole, "choiceLabels"}};
}

void SettingsModel::refreshAll()
{
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1));
}

void SettingsModel::refreshRow(const QString &id)
{
    const auto &s = ConfigSchema::settings();
    for (int i = 0; i < s.size(); ++i)
        if (s.at(i).id == id)
            emit dataChanged(index(i), index(i));
}

} // namespace urdm::app
