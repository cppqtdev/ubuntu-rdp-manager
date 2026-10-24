#include "DBusTypes.h"

#include <QDBusMetaType>

namespace urdm {

void registerDBusTypes()
{
    qDBusRegisterMetaType<DBusConfigChange>();
    qDBusRegisterMetaType<QList<DBusConfigChange>>();
}

} // namespace urdm
