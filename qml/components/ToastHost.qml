import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

// Non-blocking notifications stacked in the bottom-right corner.
Item {
    id: root

    function show(level, text) {
        toasts.append({ level: level, text: text })
        if (toasts.count > 4)
            toasts.remove(0)
    }

    ListModel {
        id: toasts
    }

    ColumnLayout {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.xl
        spacing: Theme.s
        width: 380

        Repeater {
            model: toasts
            delegate: Rectangle {
                id: toast
                required property int index
                required property string level
                required property string text

                Layout.fillWidth: true
                implicitHeight: row.implicitHeight + Theme.m * 2
                radius: Theme.radiusM
                color: Theme.cardBg
                border.color: Theme.tint(Theme.statusColor(level), 0.6)
                border.width: 1

                Accessible.role: Accessible.AlertMessage
                Accessible.name: toast.text

                Rectangle {
                    width: 4
                    radius: 2
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    color: Theme.statusColor(toast.level)
                }
                RowLayout {
                    id: row
                    anchors.fill: parent
                    anchors.margins: Theme.m
                    anchors.leftMargin: Theme.l
                    spacing: Theme.s
                    IconImage {
                        source: Theme.icon(toast.level === "error" ? "error" : toast.level === "warning" ? "warning"
                                           : toast.level === "success" ? "check" : "info")
                        sourceSize: Qt.size(18, 18)
                        color: Theme.statusColor(toast.level)
                        Layout.alignment: Qt.AlignTop
                    }
                    Label {
                        text: toast.text
                        wrapMode: Text.Wrap
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        icon.source: Theme.icon("close")
                        icon.width: 14
                        icon.height: 14
                        Accessible.name: qsTr("Dismiss notification")
                        onClicked: toasts.remove(toast.index)
                    }
                }
                Timer {
                    // errors stay longer; nothing is lost because logs keep the details
                    interval: toast.level === "error" ? 12000 : 6000
                    running: true
                    onTriggered: if (toast.index >= 0 && toast.index < toasts.count) toasts.remove(toast.index)
                }
            }
        }
    }
}
