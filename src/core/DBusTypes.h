#pragma once

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>

namespace urdm {

// D-Bus struct (sss) used by com.urdm.Helper1.ApplyConfigChanges.
struct DBusConfigChange
{
    QString section;
    QString key;
    QString value;
};

inline QDBusArgument &operator<<(QDBusArgument &arg, const DBusConfigChange &c)
{
    arg.beginStructure();
    arg << c.section << c.key << c.value;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, DBusConfigChange &c)
{
    arg.beginStructure();
    arg >> c.section >> c.key >> c.value;
    arg.endStructure();
    return arg;
}

void registerDBusTypes();

} // namespace urdm

Q_DECLARE_METATYPE(urdm::DBusConfigChange)
Q_DECLARE_METATYPE(QList<urdm::DBusConfigChange>)
