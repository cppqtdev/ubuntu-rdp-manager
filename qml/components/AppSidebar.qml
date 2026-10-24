import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

// Collapsible navigation pane (WinUI NavigationView "left" mode).
Rectangle {
    id: root

    property var pages: []
    property int currentIndex: 0
    property bool collapsed: false
    signal navigate(int index)
    signal toggle()

    implicitWidth: collapsed ? Theme.sidebarCollapsedWidth : Theme.sidebarWidth
    color: Theme.sidebarBg
    Behavior on implicitWidth { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.s
        anchors.bottomMargin: Theme.s
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.s
            Layout.rightMargin: Theme.s
            Layout.bottomMargin: Theme.s
            spacing: Theme.s

            ToolButton {
                icon.source: Theme.icon("menu")
                icon.width: 18
                icon.height: 18
                onClicked: root.toggle()
                Accessible.name: root.collapsed ? qsTr("Show menu names") : qsTr("Hide menu names")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
            }
            Image {
                visible: !root.collapsed
                source: Theme.icon("ubuntu-rdp-manager")
                sourceSize: Qt.size(22, 22)
            }
            Label {
                visible: !root.collapsed
                text: qsTr("Remote Desktop")
                font.pixelSize: Theme.body + 1
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Repeater {
            model: root.pages
            delegate: ItemDelegate {
                id: navItem
                required property int index
                required property var modelData

                Layout.fillWidth: true
                Layout.leftMargin: Theme.xs
                Layout.rightMargin: Theme.xs
                implicitHeight: 38
                highlighted: root.currentIndex === index
                spacing: Theme.m
                leftPadding: Theme.m
                text: root.collapsed ? "" : modelData.title.replace("&", "&&") // "&" would be a mnemonic
                icon.source: Theme.icon(modelData.icon)
                icon.width: 18
                icon.height: 18
                icon.color: highlighted ? Theme.accent : Theme.textSecondary
                font.pixelSize: Theme.body
                onClicked: root.navigate(index)
                Accessible.name: modelData.title
                ToolTip.visible: root.collapsed && hovered
                ToolTip.text: modelData.title
                ToolTip.delay: 300

                // WinUI selection indicator
                Rectangle {
                    visible: navItem.highlighted
                    width: 3
                    height: 16
                    radius: 1.5
                    color: Theme.accent
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                }
                // badge (e.g. failing checks)
                Rectangle {
                    visible: (navItem.modelData.badge || 0) > 0
                    width: 18
                    height: 18
                    radius: 9
                    color: Theme.danger
                    anchors.right: parent.right
                    anchors.rightMargin: root.collapsed ? 4 : Theme.m
                    anchors.verticalCenter: root.collapsed ? undefined : parent.verticalCenter
                    anchors.top: root.collapsed ? parent.top : undefined
                    Label {
                        anchors.centerIn: parent
                        text: navItem.modelData.badge || ""
                        color: Theme.dark ? "#1B1B1B" : "white"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // theme switcher (applies after restart; see main.cpp)
        ItemDelegate {
            visible: App.themeRestartRequired
            Layout.fillWidth: true
            Layout.leftMargin: Theme.xs
            Layout.rightMargin: Theme.xs
            implicitHeight: 38
            spacing: Theme.m
            leftPadding: Theme.m
            text: root.collapsed ? "" : qsTr("Restart app to change appearance")
            icon.source: Theme.icon("restart")
            icon.width: 18
            icon.height: 18
            icon.color: Theme.accent
            Accessible.name: qsTr("Restart to apply the theme")
            onClicked: App.restartApplication()
        }
        ItemDelegate {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.xs
            Layout.rightMargin: Theme.xs
            implicitHeight: 38
            spacing: Theme.m
            leftPadding: Theme.m
            text: root.collapsed ? "" : (App.themeMode === "system" ? qsTr("Appearance: automatic")
                                         : App.themeMode === "dark" ? qsTr("Appearance: dark") : qsTr("Appearance: light"))
            icon.source: Theme.icon(Theme.dark ? "moon" : "sun")
            icon.width: 18
            icon.height: 18
            icon.color: Theme.textSecondary
            Accessible.name: qsTr("Change appearance (light or dark)")
            onClicked: App.themeMode = App.themeMode === "system" ? "light" : App.themeMode === "light" ? "dark" : "system"
        }
    }
}
