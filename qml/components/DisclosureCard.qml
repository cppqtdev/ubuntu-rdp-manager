import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

// Fluent-style disclosure surface for secondary or technical information.
Pane {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconName: ""
    property bool expanded: false
    default property alias content: body.data

    padding: 0
    Layout.fillWidth: true
    background: Rectangle {
        color: Theme.cardBg
        radius: Theme.radiusS
        border.color: Theme.cardBorder
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Button {
            id: header
            Layout.fillWidth: true
            implicitHeight: 72
            padding: Theme.l
            flat: true
            hoverEnabled: false
            Accessible.name: root.title + (root.subtitle !== "" ? ". " + root.subtitle : "")
            Accessible.description: root.expanded ? qsTr("Expanded") : qsTr("Collapsed")
            onClicked: root.expanded = !root.expanded
            background: Rectangle {
                color: "transparent"
                radius: Theme.radiusS
                border.color: header.visualFocus ? Theme.accent : "transparent"
                border.width: header.visualFocus ? 1 : 0
            }

            contentItem: RowLayout {
                spacing: Theme.m
                IconImage {
                    visible: root.iconName !== ""
                    source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
                    sourceSize: Qt.size(18, 18)
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignVCenter
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0
                    Label {
                        text: root.title
                        font.pixelSize: Theme.body
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
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
                IconImage {
                    source: Theme.icon("chevron-down")
                    sourceSize: Qt.size(16, 16)
                    color: Theme.textSecondary
                    rotation: root.expanded ? 180 : 0
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
            visible: root.expanded
        }

        ColumnLayout {
            id: body
            visible: root.expanded
            Layout.fillWidth: true
            Layout.leftMargin: Theme.l
            Layout.rightMargin: Theme.l
            Layout.topMargin: Theme.m
            Layout.bottomMargin: Theme.l
            spacing: Theme.s
        }
    }
}
