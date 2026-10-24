import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

// Rounded surface used for every content group. Children go into the inner ColumnLayout.
Pane {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconName: ""
    property alias headerActions: actionsRow.data
    default property alias content: body.data

    padding: Theme.l
    background: Rectangle {
        color: Theme.cardBg
        radius: Theme.radiusL
        border.color: Theme.cardBorder
        border.width: 1
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        spacing: Theme.m

        RowLayout {
            visible: root.title !== "" || actionsRow.children.length > 0
            Layout.fillWidth: true
            spacing: Theme.s

            IconImage {
                visible: root.iconName !== ""
                source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
                sourceSize: Qt.size(20, 20)
                color: Theme.textSecondary
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    text: root.title
                    font.pixelSize: Theme.body + 2
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: root.subtitle !== ""
                    text: root.subtitle
                    font.pixelSize: Theme.caption
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                id: actionsRow
                spacing: Theme.s
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s
        }
    }
}
