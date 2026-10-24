#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

// Contract between the GUI and the privileged helper (D-Bus system service).
namespace urdm::protocol {

inline const QString kServiceName = QStringLiteral("com.urdm.Helper1");
inline const QString kObjectPath = QStringLiteral("/com/urdm/Helper1");
inline const QString kInterface = QStringLiteral("com.urdm.Helper1");

// D-Bus error names returned by the helper.
inline const QString kErrNotAuthorized = QStringLiteral("com.urdm.Helper1.Error.NotAuthorized");
inline const QString kErrInvalidArgument = QStringLiteral("com.urdm.Helper1.Error.InvalidArgument");
inline const QString kErrFailed = QStringLiteral("com.urdm.Helper1.Error.Failed");
inline const QString kErrBusy = QStringLiteral("com.urdm.Helper1.Error.Busy");

// polkit actions (packaging/polkit/com.urdm.policy)
inline const QString kActionServiceControl = QStringLiteral("com.urdm.service.control");
inline const QString kActionConfigModify = QStringLiteral("com.urdm.config.modify");
inline const QString kActionSessionsManage = QStringLiteral("com.urdm.sessions.manage");
inline const QString kActionSessionsInspect = QStringLiteral("com.urdm.sessions.inspect");
inline const QString kActionAccessManage = QStringLiteral("com.urdm.access.manage");
inline const QString kActionPackagesInstall = QStringLiteral("com.urdm.packages.install");
inline const QString kActionLogsRead = QStringLiteral("com.urdm.logs.read");

// Units verified on Ubuntu 24.04 (xrdp 0.9.24-4ubuntu0.1): xrdp.service Requires
// xrdp-sesman.service, which BindsTo xrdp.service.
inline const QString kXrdpUnit = QStringLiteral("xrdp.service");
inline const QString kSesmanUnit = QStringLiteral("xrdp-sesman.service");
inline const QString kSesmanPamService = QStringLiteral("xrdp-sesman"); // logind Service= of RDP sessions

inline const QStringList kServiceActions{QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("restart"),
                                         QStringLiteral("enable"), QStringLiteral("disable")};

// Installable components -> Ubuntu packages. The helper never installs anything else.
inline const QHash<QString, QStringList> &installComponents()
{
    static const QHash<QString, QStringList> c{
        {QStringLiteral("xrdp"), {QStringLiteral("xrdp")}},
        {QStringLiteral("xorgxrdp"), {QStringLiteral("xorgxrdp")}},
        {QStringLiteral("xfce"), {QStringLiteral("xfce4"), QStringLiteral("xfce4-terminal"), QStringLiteral("dbus-x11")}},
        {QStringLiteral("test-client"), {QStringLiteral("freerdp3-x11")}},
    };
    return c;
}

// Desktop identifiers accepted by ConfigureDesktop.
inline const QStringList kDesktopIds{QStringLiteral("system-default"), QStringLiteral("xfce")};

} // namespace urdm::protocol
