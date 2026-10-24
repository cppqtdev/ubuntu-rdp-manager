import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    property ConfirmDialog confirm

    readonly property RdpServerManager server: App.server
    readonly property RdpConfigurationManager config: App.config
    readonly property bool canAct: App.helperAvailable && server.installed && !server.busy

    title: qsTr("Turn On / Off")
    subtitle: qsTr("Switch Remote Desktop on or off, and choose whether it starts by itself when this computer starts.")

    actions: [
        Button {
            text: qsTr("Refresh")
            icon.source: Theme.icon("refresh")
            onClicked: page.server.refresh()
        }
    ]

    InlineMessage {
        visible: !page.server.installed
        severity: "info"
        title: qsTr("Remote Desktop is not set up yet")
        text: qsTr("Open Setup first.")
    }
    InlineMessage {
        visible: page.server.lastError !== ""
        severity: "error"
        title: qsTr("That did not work")
        text: page.server.lastError
    }
    InlineMessage {
        visible: page.server.portConflict
        severity: "error"
        title: qsTr("Another program is blocking Remote Desktop")
        text: qsTr("A different program is already using the remote desktop port. Remote Desktop cannot turn on until that program is switched off (often it is GNOME's own Remote Desktop in Ubuntu Settings → System), or until you choose another port in Settings.")
    }
    InlineMessage {
        visible: page.config.restartRequired
        severity: "info"
        title: qsTr("Press \"Restart\" to use your new settings")
        text: qsTr("Best done when nobody is connected.")
    }

    Card {
        Layout.fillWidth: true
        title: Words.serverHeadline(page.server.plainStatus)
        subtitle: Words.serverExplanation(page.server.plainStatus)
        iconName: "power"
        headerActions: [
            StatusPill {
                status: Words.statusKey(page.server.plainStatus)
                text: page.server.plainStatus === "on" ? qsTr("On") : page.server.plainStatus === "off" ? qsTr("Off")
                    : page.server.plainStatus === "problem" ? qsTr("Problem") : page.server.plainStatus === "not-installed" ? qsTr("Not set up") : qsTr("Please wait")
                busy: page.server.busy || page.server.plainStatus === "starting" || page.server.plainStatus === "stopping"
            }
        ]

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s

            Button {
                text: qsTr("Turn on")
                icon.source: Theme.icon("play")
                highlighted: true
                enabled: page.canAct && !page.server.running
                onClicked: page.server.start()
            }
            Button {
                text: qsTr("Turn off")
                icon.source: Theme.icon("stop")
                enabled: page.canAct && page.server.running
                onClicked: page.confirm.ask(qsTr("Turn off Remote Desktop?"),
                    App.sessions.connectedCount > 0
                        ? qsTr("People connected right now: %1. They will be disconnected, and nobody can connect until you turn it on again.").arg(App.sessions.connectedCount)
                        : qsTr("Nobody can connect until you turn it on again."),
                    App.sessions.totalCount > 0
                        ? qsTr("Open desktops (%1) are not closed, but after turning it back on people get a fresh desktop and cannot get back to the old one. Ask them to save their work and sign out first.").arg(App.sessions.totalCount)
                        : "",
                    qsTr("Turn off"), App.sessions.totalCount > 0, () => page.server.stop())
            }
            Button {
                text: qsTr("Restart")
                icon.source: Theme.icon("restart")
                enabled: page.canAct && page.server.running
                onClicked: page.confirm.ask(qsTr("Restart Remote Desktop?"),
                    qsTr("This applies saved settings. People who are connected are disconnected for a moment."),
                    App.sessions.totalCount > 0
                        ? qsTr("Open desktops (%1) cannot be reached again after a restart — people get a fresh desktop. Best done when nobody is connected.").arg(App.sessions.totalCount)
                        : "",
                    qsTr("Restart"), App.sessions.totalCount > 0, () => page.server.restart())
            }
            Item { Layout.fillWidth: true }
            Switch {
                text: qsTr("Start automatically when this computer starts")
                checked: page.server.autostart
                enabled: page.canAct
                onToggled: page.server.setAutostart(checked)
            }
        }
        Label {
            text: qsTr("You will be asked for your password. This makes sure only administrators can change Remote Desktop.")
            color: Theme.textTertiary
            font.pixelSize: Theme.caption + 1
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Network")
        iconName: "network"
        KeyValue { key: qsTr("People connect to"); value: (page.server.connectAddresses.length ? page.server.connectAddresses.join(", ") : page.config.hostName)
                                                         + (page.config.primaryPort !== 3389 ? qsTr("  (port %1)").arg(page.config.primaryPort) : "") }
        KeyValue { key: qsTr("Ready for connections"); value: page.server.listening ? qsTr("Yes") : qsTr("No") }
        InlineMessage {
            visible: page.config.exposesNonLoopback
            severity: "warning"
            title: qsTr("Keep Remote Desktop inside your own network")
            text: qsTr("Any computer that can reach this one may try to sign in. Use it only in your office network or through a VPN, and never make port %1 reachable from the internet. Your IT person can set a firewall rule, for example: sudo ufw allow from 192.168.1.0/24 to any port %1 proto tcp").arg(page.config.primaryPort)
        }
    }

    Button {
        text: details.visible ? qsTr("Hide technical details") : qsTr("Show technical details")
        flat: true
        icon.source: Theme.icon("info")
        onClicked: details.visible = !details.visible
    }
    Card {
        id: details
        visible: false
        Layout.fillWidth: true
        title: qsTr("Technical details")
        iconName: "settings"
        KeyValue { key: qsTr("Service"); value: "xrdp.service — " + page.server.activeState + " (" + page.server.subState + ")" }
        KeyValue { key: qsTr("Session manager"); value: "xrdp-sesman.service — " + page.server.sesmanStatusText }
        KeyValue { key: qsTr("Last result"); value: page.server.unitResult }
        KeyValue { key: qsTr("Process ID"); value: page.server.mainPid > 0 ? String(page.server.mainPid) : "" }
        KeyValue { key: qsTr("Running for"); value: page.server.uptimeText }
        KeyValue { key: qsTr("Software"); value: "xrdp " + (page.config.xrdpPackageVersion || "—") + ", xorgxrdp " + (page.config.xorgxrdpPackageVersion || "—") }
        KeyValue { key: qsTr("Configured listen setting"); value: page.config.listenSpec }
        KeyValue { key: qsTr("Listening on"); value: page.server.listeningEndpoints.join(", ") }
        KeyValue { key: qsTr("All network addresses"); value: page.server.ipAddresses.join("\n") }
    }
}
