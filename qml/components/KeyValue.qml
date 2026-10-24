import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

// "Label ........ value" row used in detail cards.
RowLayout {
    id: root

    property string key: ""
    property string value: ""
    property bool selectable: false
    property int keyWidth: 180

    Layout.fillWidth: true
    spacing: Theme.m

    Label {
        text: root.key
        color: Theme.textSecondary
        font.pixelSize: Theme.body
        Layout.preferredWidth: root.keyWidth
        Layout.alignment: Qt.AlignTop
        elide: Text.ElideRight
    }
    TextEdit {
        visible: root.selectable
        text: root.value
        readOnly: true
        selectByMouse: true
        color: Theme.textPrimary
        selectionColor: Theme.accent
        font.pixelSize: Theme.body
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }
    Label {
        visible: !root.selectable
        text: root.value === "" ? "—" : root.value
        color: Theme.textPrimary
        font.pixelSize: Theme.body
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
