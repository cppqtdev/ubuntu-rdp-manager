import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

// WinUI-style InfoBar for page-level information, warnings and errors.
Rectangle {
    id: root

    property string severity: "info"   // info, success, warning, error
    property string title: ""
    property string text: ""
    property string actionText: ""
    signal actionTriggered()

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.m * 2
    radius: Theme.radiusM
    color: Theme.tint(Theme.statusColor(severity), Theme.dark ? 0.16 : 0.09)
    border.color: Theme.tint(Theme.statusColor(severity), 0.35)

    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.title + " " + root.text

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.m
        spacing: Theme.m

        IconImage {
            source: Theme.icon(root.severity === "error" ? "error" : root.severity === "warning" ? "warning"
                               : root.severity === "success" ? "check" : "info")
            sourceSize: Qt.size(20, 20)
            color: Theme.statusColor(root.severity)
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                visible: root.title !== ""
                text: root.title
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                visible: root.text !== ""
                text: root.text
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        Button {
            visible: root.actionText !== ""
            text: root.actionText
            onClicked: root.actionTriggered()
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
