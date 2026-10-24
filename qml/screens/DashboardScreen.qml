import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    signal openPage(int index)

    readonly property RdpServerManager server: App.server
    readonly property RdpConfigurationManager config: App.config
    readonly property SessionManager sessions: App.sessions
    readonly property SystemMonitor monitor: App.monitor
    readonly property bool canAct: App.helperAvailable && server.installed && !server.busy
    property bool showDetails: false

    title: qsTr("Overview")
    subtitle: qsTr("Remote Desktop lets other people use this computer (%1) from their own computer, each with their own separate desktop.")
              .arg(config.hostName)

    actions: [
        Button {
            text: qsTr("Refresh")
            icon.source: Theme.icon("refresh")
            onClicked: { page.server.refresh(); page.sessions.refresh(); App.logs.reload() }
        }
    ]

    // ---- the one thing that matters most -----------------------------------------------
    Pane {
        Layout.fillWidth: true
        padding: Theme.xl
        background: Rectangle {
            radius: Theme.radiusL
            color: Theme.tint(Theme.statusColor(Words.statusKey(page.server.plainStatus)), Theme.dark ? 0.16 : 0.08)
            border.color: Theme.tint(Theme.statusColor(Words.statusKey(page.server.plainStatus)), 0.5)
        }
        RowLayout {
            anchors.fill: parent
            spacing: Theme.xl
            IconImage {
                source: Theme.icon(page.server.plainStatus === "on" ? "check" : page.server.plainStatus === "problem" ? "error" : "power")
                sourceSize: Qt.size(48, 48)
                color: Theme.statusColor(Words.statusKey(page.server.plainStatus))
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.xs
                Label {
                    text: Words.serverHeadline(page.server.plainStatus)
                    font.pixelSize: Theme.title
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Accessible.role: Accessible.Heading
                }
                Label {
                    text: Words.serverExplanation(page.server.plainStatus)
                          + (page.server.plainStatus === "on" && page.server.autostart ? " " + qsTr("It also turns on by itself when this computer starts.") : "")
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            Switch {
                visible: page.server.installed
                checked: page.server.plainStatus === "on" || page.server.plainStatus === "starting"
                enabled: page.canAct
                text: checked ? qsTr("On") : qsTr("Off")
                font.pixelSize: Theme.subtitle
                Accessible.name: qsTr("Allow remote connections")
                onToggled: {
                    const wanted = checked
                    checked = Qt.binding(() => page.server.plainStatus === "on" || page.server.plainStatus === "starting")
                    if (wanted)
                        page.server.setRemoteDesktopEnabled(true)
                    else
                        page.confirmOff()
                }
            }
            Button {
                visible: !page.server.installed && page.server.plainStatus === "not-installed"
                text: qsTr("Set up now")
                highlighted: true
                onClicked: page.openPage(1)
            }
        }
    }

    function confirmOff() {
        App.sessions.totalCount > 0
            ? confirm.ask(qsTr("Turn off Remote Desktop?"),
                          qsTr("People connected right now: %1. They will be disconnected, and nobody can connect until you turn it on again.").arg(App.sessions.connectedCount),
                          qsTr("Their open programs keep running, but after turning it back on they get a fresh desktop. Ask them to save their work and sign out first."),
                          qsTr("Turn off"), true, () => page.server.setRemoteDesktopEnabled(false))
            : confirm.ask(qsTr("Turn off Remote Desktop?"),
                          qsTr("Nobody will be able to connect until you turn it on again. It will also stay off after this computer restarts."),
                          "", qsTr("Turn off"), false, () => page.server.setRemoteDesktopEnabled(false))
    }
    ConfirmDialog { id: confirm }

    InlineMessage {
        visible: page.server.lastError !== ""
        severity: "error"
        title: qsTr("That did not work")
        text: page.server.lastError
    }
    InlineMessage {
        visible: page.config.limitKind === "denies-all"
        severity: "error"
        title: qsTr("Nobody can connect right now")
        text: page.config.limitDescription
        actionText: qsTr("Fix it")
        onActionTriggered: page.openPage(5)
    }
    InlineMessage {
        visible: App.diagnostics.failCount > 0
        severity: "warning"
        title: App.diagnostics.failCount === 1 ? qsTr("1 problem found") : qsTr("%1 problems found").arg(App.diagnostics.failCount)
        text: qsTr("The Health Check explains what is wrong and how to fix it.")
        actionText: qsTr("Open Health Check")
        onActionTriggered: page.openPage(9)
    }
    InlineMessage {
        visible: page.config.restartRequired
        severity: "info"
        title: qsTr("Restart Remote Desktop to use your new settings")
        text: qsTr("Best done when nobody is connected, because people who are connected will get a fresh desktop.")
        actionText: qsTr("Go to Turn On / Off")
        onActionTriggered: page.openPage(2)
    }

    // ---- how to connect ----------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        visible: page.server.installed
        title: qsTr("How to connect from another computer")
        iconName: "link"

        GridLayout {
            columns: page.width > 900 ? 2 : 1
            columnSpacing: Theme.xxl
            rowSpacing: Theme.m
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.s
                Label { text: qsTr("Type this address in the remote desktop program:"); color: Theme.textSecondary }
                Repeater {
                    model: page.server.connectAddresses.length > 0 ? page.server.connectAddresses : [page.config.hostName]
                    delegate: TextEdit {
                        required property string modelData
                        text: modelData + (page.config.primaryPort !== 3389 ? ":" + page.config.primaryPort : "")
                        readOnly: true
                        selectByMouse: true
                        font.pixelSize: Theme.subtitle + 4
                        font.weight: Font.DemiBold
                        color: Theme.accent
                        Accessible.name: qsTr("Address to connect to")
                    }
                }
                Label {
                    text: qsTr("Or use this computer's name: %1").arg(page.config.hostName)
                    color: Theme.textTertiary
                }
                Label {
                    text: qsTr("Then sign in with your Ubuntu user name and password.")
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.xs
                Label { text: qsTr("Windows"); font.weight: Font.DemiBold; color: Theme.textPrimary }
                Label { text: qsTr("Press the Windows key, type \"Remote Desktop Connection\", open it, enter the address and click Connect."); color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Label { text: qsTr("Mac, iPhone, iPad, Android"); font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.topMargin: Theme.s }
                Label { text: qsTr("Install Microsoft's free \"Windows App\" (formerly Remote Desktop) from the app store and add a PC with the address."); color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Label { text: qsTr("Linux"); font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.topMargin: Theme.s }
                Label { text: qsTr("Use Remmina (preinstalled on Ubuntu) and choose the RDP option."); color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Label {
                    text: qsTr("The first time, the program may ask whether to trust this computer's certificate. That is normal — you can compare its fingerprint on the Settings page.")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.caption + 1
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.s
                }
            }
        }
    }

    // ---- at a glance -------------------------------------------------------------------
    GridLayout {
        Layout.fillWidth: true
        columns: page.width >= 1000 ? 4 : 2
        columnSpacing: Theme.l
        rowSpacing: Theme.l

        MetricTile {
            label: qsTr("People connected now")
            iconName: "users"
            value: page.sessions.connectedCount
            caption: page.sessions.disconnectedCount > 0 ? (page.sessions.disconnectedCount === 1 ? qsTr("+ 1 desktop left open") : qsTr("+ %1 desktops left open").arg(page.sessions.disconnectedCount))
                                                          : qsTr("No desktops left open")
            status: page.sessions.connectedCount > 0 ? "ok" : ""
        }
        MetricTile {
            label: qsTr("How many can connect")
            iconName: "limits"
            value: page.config.limitKind === "unlimited" ? qsTr("No limit")
                 : page.config.limitKind === "limited" ? qsTr("Up to %1").arg(page.config.maxSessions)
                 : page.config.limitKind === "denies-all" ? qsTr("Nobody!") : "—"
            status: page.config.limitKind === "denies-all" || page.config.limitKind === "invalid" ? "error" : ""
            caption: page.monitor.hasData ? qsTr("Room for about %1 more right now").arg(page.monitor.estimatedAdditionalSessions) : ""
        }
        MetricTile {
            label: qsTr("Computer is busy")
            iconName: "cpu"
            value: page.monitor.hasData ? page.monitor.cpuPercent.toFixed(0) + " %" : "…"
            progress: page.monitor.cpuPercent / 100
            caption: page.monitor.cpuPercent < 60 ? qsTr("Plenty of processing power left") : qsTr("Working hard")
        }
        MetricTile {
            label: qsTr("Memory in use")
            iconName: "memory"
            value: page.monitor.hasData ? page.monitor.memUsedPercent.toFixed(0) + " %" : "…"
            progress: page.monitor.memUsedPercent / 100
            caption: qsTr("%1 free of %2").arg(Theme.formatMiB(page.monitor.memAvailableMiB)).arg(Theme.formatMiB(page.monitor.memTotalMiB))
            status: page.monitor.advisorLevel === "ok" ? "" : page.monitor.advisorLevel
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.l

        Card {
            title: qsTr("Who is using this computer")
            iconName: "users"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            headerActions: [ Button { text: qsTr("Manage"); flat: true; onClicked: page.openPage(3) } ]

            EmptyState {
                visible: page.sessions.totalCount === 0
                iconName: "sessions"
                title: qsTr("Nobody is connected")
                text: page.server.running ? qsTr("When someone signs in, they appear here.") : qsTr("Remote Desktop is off.")
            }
            Repeater {
                model: page.sessions.model
                delegate: RowLayout {
                    required property string userName
                    required property string sessionState
                    required property string durationText
                    Layout.fillWidth: true
                    spacing: Theme.m
                    IconImage { source: Theme.icon("user"); sourceSize: Qt.size(16, 16); color: Theme.textSecondary }
                    Label { text: userName; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.preferredWidth: 160; elide: Text.ElideRight }
                    StatusPill { status: sessionState; text: Words.sessionState(sessionState) }
                    Item { Layout.fillWidth: true }
                    Label { text: qsTr("for %1").arg(durationText); color: Theme.textTertiary }
                }
            }
        }

        Card {
            title: qsTr("Problems in the last 24 hours")
            iconName: "warning"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            headerActions: [ Button { text: qsTr("Activity Log"); flat: true; onClicked: page.openPage(8) } ]

            EmptyState {
                visible: App.logs.recentProblems.length === 0
                iconName: "check"
                title: App.logs.needsElevation ? qsTr("Can't read the activity log yet") : qsTr("No problems")
                text: App.logs.needsElevation ? qsTr("Open the Activity Log to show it (asks for your password).") : ""
            }
            Repeater {
                model: App.logs.recentProblems.slice(0, 5)
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0
                    RowLayout {
                        spacing: Theme.s
                        StatusPill { status: "warning"; text: Words.logCategory(modelData.category) }
                        Label { text: modelData.time; color: Theme.textTertiary }
                    }
                    Label { text: modelData.message; color: Theme.textSecondary; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: Theme.caption + 1 }
                }
            }
        }
    }

    // ---- technical details (folded away) -------------------------------------------------
    Button {
        text: page.showDetails ? qsTr("Hide technical details") : qsTr("Show technical details")
        flat: true
        icon.source: Theme.icon("info")
        onClicked: page.showDetails = !page.showDetails
    }
    Card {
        visible: page.showDetails
        Layout.fillWidth: true
        title: qsTr("Technical details")
        subtitle: qsTr("For IT staff and support.")
        iconName: "settings"
        KeyValue { key: qsTr("Server software"); value: "xrdp " + (page.config.xrdpPackageVersion || qsTr("not installed")) + ", xorgxrdp " + (page.config.xorgxrdpPackageVersion || "—") }
        KeyValue { key: qsTr("Service state"); value: page.server.activeState + " / " + page.server.subState + " · " + qsTr("up %1").arg(page.server.uptimeText) }
        KeyValue { key: qsTr("Listening on"); value: page.server.listening ? page.server.listeningEndpoints.join(", ") : qsTr("not listening") }
        KeyValue { key: qsTr("Encryption"); value: page.config.securityLayer + (page.config.certificateInfo.selfSigned ? qsTr(" · self-made certificate") : "") }
        KeyValue { key: qsTr("Desktop"); value: page.config.desktopId === "xfce" ? "Xfce" : page.config.desktopScript }
        KeyValue { key: qsTr("All network addresses"); value: page.server.ipAddresses.join("\n") }
        KeyValue { key: qsTr("Memory used by remote desktops"); value: Theme.formatMiB(page.monitor.sessionMemoryMiB) }
        KeyValue { key: qsTr("Computer running for"); value: page.monitor.systemUptimeText }
    }
}
