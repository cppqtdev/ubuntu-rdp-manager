import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

ColumnLayout {
    id: root

    property string iconName: "info"
    property string title: ""
    property string text: ""

    spacing: Theme.s
    Layout.fillWidth: true
    Layout.topMargin: Theme.xl
    Layout.bottomMargin: Theme.xl

    IconImage {
        source: Theme.icon(root.iconName)
        sourceSize: Qt.size(40, 40)
        color: Theme.textTertiary
        Layout.alignment: Qt.AlignHCenter
    }
    Label {
        text: root.title
        font.pixelSize: Theme.body + 2
        font.weight: Font.DemiBold
        color: Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
    Label {
        visible: root.text !== ""
        text: root.text
        color: Theme.textSecondary
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
}
