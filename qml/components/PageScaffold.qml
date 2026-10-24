import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

// Standard page: title row with actions, then scrollable content constrained to a readable width.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property alias actions: actionRow.data
    default property alias content: contentColumn.data
    property bool scrollable: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.xxl
            Layout.rightMargin: Theme.xxl
            Layout.topMargin: Theme.xl
            Layout.bottomMargin: Theme.l
            spacing: Theme.m

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    text: root.title
                    font.pixelSize: Theme.title
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Accessible.role: Accessible.Heading
                }
                Label {
                    visible: root.subtitle !== ""
                    text: root.subtitle
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                id: actionRow
                spacing: Theme.s
                Layout.alignment: Qt.AlignVCenter
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            enabled: true

            Item {
                width: scroll.availableWidth
                implicitHeight: contentColumn.implicitHeight + Theme.xxl

                ColumnLayout {
                    id: contentColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width - Theme.xxl * 2, Theme.contentMaxWidth)
                    spacing: Theme.l
                }
            }
        }
    }
}
