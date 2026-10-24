import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

// Coloured status badge. Colour is never the only signal: the text always states the status.
Rectangle {
    id: root

    property string status: "neutral"
    property string text: ""
    property bool busy: false

    implicitHeight: 24
    implicitWidth: row.implicitWidth + Theme.m * 2
    radius: height / 2
    color: Theme.tint(Theme.statusColor(status), Theme.dark ? 0.18 : 0.12)
    border.color: Theme.tint(Theme.statusColor(status), 0.45)
    border.width: 1

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Theme.xs + 2

        BusyIndicator {
            visible: root.busy
            running: root.busy
            implicitWidth: 14
            implicitHeight: 14
            padding: 0
        }
        Rectangle {
            visible: !root.busy
            width: 8
            height: 8
            radius: 4
            color: Theme.statusColor(root.status)
        }
        Label {
            text: root.text
            font.pixelSize: Theme.caption
            font.weight: Font.DemiBold
            color: Theme.statusColor(root.status)
        }
    }
}
