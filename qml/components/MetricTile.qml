import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

// Compact KPI tile: label, large value, optional caption and status accent.
Pane {
    id: root

    property string label: ""
    property string value: ""
    property string caption: ""
    property string iconName: ""
    property string status: ""      // optional: colours the accent bar
    property real progress: -1      // 0..1 shows a usage bar

    padding: Theme.l
    Layout.fillWidth: true
    Layout.minimumWidth: 180
    Layout.preferredWidth: 240
    Layout.horizontalStretchFactor: 1
    implicitHeight: 118

    background: Rectangle {
        color: Theme.cardBg
        radius: Theme.radiusS
        border.color: Theme.cardBorder
        Rectangle {
            visible: root.status !== ""
            width: 3
            radius: 1.5
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: Theme.m
            color: Theme.statusColor(root.status)
        }
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: root.label + ": " + root.value + (root.caption !== "" ? ", " + root.caption : "")

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.status !== "" ? Theme.s : 0
        spacing: Theme.xs

        RowLayout {
            spacing: Theme.s
            IconImage {
                visible: root.iconName !== ""
                source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
                sourceSize: Qt.size(16, 16)
                color: Theme.textSecondary
            }
            Label {
                text: root.label
                font.pixelSize: Theme.caption
                color: Theme.textSecondary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        Label {
            text: root.value
            font.pixelSize: Theme.subtitle + 4
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        ProgressBar {
            visible: root.progress >= 0
            from: 0
            to: 1
            value: Math.max(0, Math.min(1, root.progress))
            Layout.fillWidth: true
        }
        Label {
            visible: root.caption !== ""
            text: root.caption
            font.pixelSize: Theme.caption
            color: Theme.textTertiary
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
