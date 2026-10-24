import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    property ConfirmDialog confirm

    readonly property SessionManager sessions: App.sessions
    readonly property bool canAct: App.helperAvailable && !sessions.busy

    title: qsTr("Who's Connected")
    subtitle: qsTr("Everyone who has a remote desktop open on this computer. Each person has their own separate desktop.")

    actions: [
        Button {
            text: qsTr("Show where they connect from")
            icon.source: Theme.icon("network")
            enabled: App.helperAvailable
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Shows the network address of each person's computer (asks for your password)")
            onClicked: page.sessions.loadClientAddresses()
        },
        Button {
            text: qsTr("Refresh")
            icon.source: Theme.icon("refresh")
            onClicked: page.sessions.refresh()
        }
    ]

    GridLayout {
        Layout.fillWidth: true
        columns: page.width >= 1000 ? 4 : 2
        columnSpacing: Theme.l
        rowSpacing: Theme.l
        MetricTile {
            label: qsTr("Open desktops")
            value: page.sessions.totalCount
            iconName: "sessions"
        }
        MetricTile {
            label: qsTr("Connected now")
            value: page.sessions.connectedCount
            status: "ok"
            iconName: "link"
        }
        MetricTile { label: qsTr("Away – still open"); value: page.sessions.disconnectedCount - page.sessions.orphanedCount; status: "warning"; iconName: "disconnect"
                     caption: qsTr("They get back to it by signing in again") }
        MetricTile { label: qsTr("Left open / stuck"); value: page.sessions.staleCount; status: page.sessions.staleCount > 0 ? "error" : ""; iconName: "trash"
                     caption: qsTr("Can be closed to free memory") }
    }

    InlineMessage {
        visible: page.sessions.orphanedCount > 0
        severity: "warning"
        title: qsTr("Stuck desktops: %1").arg(page.sessions.orphanedCount)
        text: qsTr("Remote Desktop was restarted while these desktops were open. They still use memory, but nobody can get back to them. It is safe to close them once you know nobody needs unsaved work in them.")
        actionText: qsTr("Close stuck desktops…")
        onActionTriggered: page.confirmCleanup()
    }
    InlineMessage {
        visible: !page.sessions.available
        severity: "error"
        title: qsTr("Cannot read the list of desktops")
        text: qsTr("Ubuntu's login service did not answer. Try Refresh in a moment.")
    }

    function confirmCleanup() {
        page.confirm.ask(qsTr("Close desktops that were left open?"),
            qsTr("Desktops to close: %1. Any unsaved work in them will be lost.").arg(page.sessions.staleCount),
            qsTr("Only stuck desktops and desktops nobody has used for more than %1 hours are closed. People who are connected are not affected.").arg(page.sessions.staleHours),
            qsTr("Close them"), true, () => page.sessions.terminateStaleSessions())
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Desktops")
        iconName: "sessions"
        headerActions: [
            Label { text: qsTr("Count as left open after"); color: Theme.textSecondary },
            SpinBox {
                from: 1; to: 720
                value: page.sessions.staleHours
                editable: true
                onValueModified: page.sessions.staleHours = value
                Accessible.name: qsTr("Hours before an unused desktop counts as left open")
            },
            Label { text: qsTr("hours"); color: Theme.textSecondary },
            Button {
                text: qsTr("Close left-open…")
                icon.source: Theme.icon("trash")
                enabled: page.canAct && page.sessions.staleCount > 0
                onClicked: page.confirmCleanup()
            }
        ]

        // header row
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s
            visible: page.sessions.totalCount > 0
            component Head: Label {
                color: Theme.textSecondary
                font.pixelSize: Theme.caption
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Head {
                text: qsTr("No.")
                Layout.preferredWidth: 70
            }
            Head {
                text: qsTr("Person")
                Layout.preferredWidth: 130
            }
            Head {
                text: qsTr("State")
                Layout.preferredWidth: 150
            }
            Head {
                text: qsTr("Screen")
                Layout.preferredWidth: 60
            }
            Head {
                text: qsTr("Signed in at")
                Layout.preferredWidth: 130
            }
            Head {
                text: qsTr("Connecting from")
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }
            Head {
                text: qsTr("Memory")
                Layout.preferredWidth: 80
                horizontalAlignment: Text.AlignRight
            }
            Head {
                text: qsTr("CPU")
                Layout.preferredWidth: 55
                horizontalAlignment: Text.AlignRight
            }
            Head {
                text: qsTr("Actions")
                Layout.preferredWidth: 120
                horizontalAlignment: Text.AlignRight
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
            visible: page.sessions.totalCount > 0
        }

        EmptyState {
            visible: page.sessions.totalCount === 0
            iconName: "sessions"
            title: qsTr("Nobody is connected")
            text: App.server.running ? qsTr("People appear here a few seconds after they sign in.")
                                     : qsTr("Remote Desktop is off, so nobody can connect.")
        }

        Repeater {
            model: page.sessions.model
            delegate: Rectangle {
                id: rowItem
                required property string sessionId
                required property string userName
                required property string sessionState
                required property string display
                required property string startedText
                required property string durationText
                required property string clientAddress
                required property string disconnectedSince
                required property real memoryMiB
                required property real cpuPercent
                required property int processCount
                required property bool stale

                Layout.fillWidth: true
                implicitHeight: 44
                radius: Theme.radiusS
                color: hover.hovered ? Theme.hoverBg : "transparent"
                HoverHandler {
                    id: hover
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.xs
                    anchors.rightMargin: Theme.xs
                    spacing: Theme.s

                    Label {
                        text: rowItem.sessionId
                        color: Theme.textSecondary
                        font.family: "monospace"
                        Layout.preferredWidth: 70
                    }
                    RowLayout {
                        Layout.preferredWidth: 130
                        Layout.fillWidth: false // nested layouts default to filling and would steal space
                        spacing: Theme.xs
                        IconImage {
                            source: Theme.icon("user")
                            sourceSize: Qt.size(14, 14)
                            color: Theme.textSecondary
                        }
                        Label {
                            text: rowItem.userName
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    Item {
                        Layout.preferredWidth: 150
                        implicitHeight: pill.implicitHeight
                        StatusPill {
                            id: pill
                            status: rowItem.sessionState
                            text: rowItem.sessionState === "Disconnected" && rowItem.disconnectedSince !== ""
                                  ? qsTr("Away since %1").arg(rowItem.disconnectedSince) : Words.sessionState(rowItem.sessionState)
                            ToolTip.visible: pillHover.hovered
                            ToolTip.text: Words.sessionStateHelp(rowItem.sessionState)
                            HoverHandler {
                                id: pillHover
                            }
                        }
                    }
                    Label {
                        text: rowItem.display
                        color: Theme.textPrimary
                        font.family: "monospace"
                        Layout.preferredWidth: 60
                    }
                    Label { text: rowItem.startedText; color: Theme.textSecondary; Layout.preferredWidth: 130
                            ToolTip.visible: startHover.hovered; ToolTip.text: qsTr("signed in %1 ago").arg(rowItem.durationText)
                            HoverHandler { id: startHover } }
                    Label {
                        text: rowItem.clientAddress !== "" ? rowItem.clientAddress
                              : rowItem.sessionState === "Connected" ? (page.sessions.addressesLoaded ? qsTr("unknown") : qsTr("press “Show where they connect from”"))
                                                              : "—"
                        color: rowItem.clientAddress !== "" ? Theme.textPrimary : Theme.textTertiary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                    }
                    Label {
                        text: Theme.formatMiB(rowItem.memoryMiB)
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 80
                    }
                    Label { text: rowItem.cpuPercent < 0 ? "—" : rowItem.cpuPercent.toFixed(1) + "%"; color: Theme.textPrimary
                            horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 55 }
                    RowLayout {
                        Layout.preferredWidth: 120
                        Layout.fillWidth: false
                        spacing: 0
                        Item {
                            Layout.fillWidth: true
                        }
                        ToolButton {
                            icon.source: Theme.icon("info")
                            icon.width: 16; icon.height: 16
                            Accessible.name: qsTr("Details of session %1").arg(rowItem.sessionId)
                            ToolTip.visible: hovered; ToolTip.text: qsTr("More information")
                            onClicked: { details.info = page.sessions.sessionDetails(rowItem.sessionId); details.open() }
                        }
                        ToolButton {
                            icon.source: Theme.icon("disconnect")
                            icon.width: 16; icon.height: 16
                            enabled: page.canAct && rowItem.sessionState === "Connected"
                            Accessible.name: qsTr("Disconnect session %1").arg(rowItem.sessionId)
                            ToolTip.visible: hovered; ToolTip.text: qsTr("Disconnect – their programs stay open")
                            onClicked: page.confirm.ask(qsTr("Disconnect %1?").arg(rowItem.userName),
                                qsTr("%1's remote desktop window will close on their computer. Their programs and documents stay open, and they can connect again to continue.").arg(rowItem.userName),
                                "", qsTr("Disconnect"), false, () => page.sessions.disconnectSession(rowItem.sessionId))
                        }
                        ToolButton {
                            icon.source: Theme.icon("trash")
                            icon.width: 16; icon.height: 16
                            icon.color: Theme.danger
                            enabled: page.canAct && rowItem.sessionState !== "Closing"
                            Accessible.name: qsTr("Terminate session %1").arg(rowItem.sessionId)
                            ToolTip.visible: hovered; ToolTip.text: qsTr("Close their desktop – closes all their programs")
                            onClicked: page.confirm.ask(qsTr("Close %1's desktop?").arg(rowItem.userName),
                                qsTr("All of %1's programs on this remote desktop will be closed. Anything they have not saved will be lost.").arg(rowItem.userName),
                                qsTr("Only this remote desktop is affected. People working directly at this computer are never touched."),
                                qsTr("Close desktop"), true, () => page.sessions.terminateSession(rowItem.sessionId))
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: details
        property var info: ({})
        title: qsTr("Desktop of %1").arg(info.userName || "")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 560
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.xs
            KeyValue {
                key: qsTr("Person")
                value: (details.info.userName || "")
            }
            KeyValue {
                key: qsTr("Status")
                value: Words.sessionState(details.info.state || "")
            }
            KeyValue {
                key: qsTr("Screen number")
                value: details.info.display || ""
            }
            KeyValue {
                key: qsTr("Signed in at")
                value: (details.info.started || "").replace("T", " ")
            }
            KeyValue {
                key: qsTr("Connecting from")
                value: details.info.clientAddress || ""
            }
            KeyValue {
                key: qsTr("Programs running")
                value: details.info.processCount >= 0 ? String(details.info.processCount) : qsTr("unknown")
            }
            KeyValue {
                key: qsTr("Memory used")
                value: Theme.formatMiB(details.info.memoryMiB)
            }
            KeyValue {
                key: qsTr("Technical ID")
                value: (details.info.sessionId || "") + " · " + (details.info.scope || "") + " · uid " + (details.info.uid || "")
                selectable: true
            }
                        KeyValue {
                            key: qsTr("Desktop type")
                            value: "Xorg (xorgxrdp)"
                        }
        }
    }
}
