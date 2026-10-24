// urdm-helper — privileged D-Bus system service for Ubuntu RDP Manager.
// Started by D-Bus activation (packaging/dbus, packaging/systemd); exits when idle.
#include "AuditLog.h"
#include "DBusTypes.h"
#include "HelperCore.h"
#include "HelperProtocol.h"
#include "HelperService.h"
#include "LinuxSystemOps.h"
#include "PolkitAuthorizer.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>

#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("urdm-helper"));
    QCoreApplication::setApplicationVersion(QStringLiteral(URDM_VERSION));

    if (::geteuid() != 0) {
        std::fprintf(stderr, "urdm-helper must run as root (it is started by D-Bus activation)\n");
        return 1;
    }
    ::umask(022);
    urdm::registerDBusTypes();

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        std::fprintf(stderr, "cannot connect to the system bus: %s\n", qPrintable(bus.lastError().message()));
        return 1;
    }

    urdm::helper::HelperPaths paths;
    paths.backupDir = QStringLiteral(URDM_STATE_DIR "/backups");
    paths.startwmDir = QStringLiteral(URDM_DATA_DIR "/startwm");

    urdm::helper::LinuxSystemOps ops(bus);
    urdm::helper::PolkitAuthorizer authorizer(bus);
    urdm::helper::AuditLog audit(QStringLiteral(URDM_LOG_DIR "/audit.log"));
    urdm::helper::HelperCore core(ops, paths);
    urdm::helper::HelperService service(core, authorizer, audit, /*idleExitSeconds*/ 120);

    if (!bus.registerObject(urdm::protocol::kObjectPath, &service,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        std::fprintf(stderr, "cannot register object: %s\n", qPrintable(bus.lastError().message()));
        return 1;
    }
    if (!bus.registerService(urdm::protocol::kServiceName)) {
        std::fprintf(stderr, "cannot own %s: %s\n", qPrintable(urdm::protocol::kServiceName),
                     qPrintable(bus.lastError().message()));
        return 1;
    }
    return QCoreApplication::exec();
}
