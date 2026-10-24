import QtQuick
import QtQuick.Controls
import QtQuick.Controls.FluentWinUI3 // selects the style at compile time and for deployment
import QtQuick.Layouts
import UbuntuRdpManager

ApplicationWindow {
    id: window

    width: 1366
    height: 768
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: qsTr("Remote Desktop Manager")
    color: Theme.windowBg

    readonly property var pages: [
        { title: qsTr("Overview"), icon: "dashboard" },
        { title: qsTr("Setup"), icon: "install" },
        { title: qsTr("Turn On / Off"), icon: "power" },
        { title: qsTr("Who's Connected"), icon: "sessions", badge: 0 },
        { title: qsTr("Who Can Connect"), icon: "users" },
        { title: qsTr("Connection Limits"), icon: "limits" },
        { title: qsTr("Settings"), icon: "settings" },
        { title: qsTr("Performance"), icon: "performance" },
        { title: qsTr("Activity Log"), icon: "logs" },
        { title: qsTr("Health Check"), icon: "diagnostics", badge: App.diagnostics.failCount },
        { title: qsTr("About & Help"), icon: "about" }
    ]

    function go(index) {
        App.currentPage = Math.max(0, Math.min(pages.length - 1, index))
    }

    // Diagnostics "action" ids map to pages
    function openAction(action) {
        const map = { "open-installation": 1, "open-server": 2, "open-sessions": 3, "open-users": 4,
                      "open-limits": 5, "open-configuration": 6, "open-tls": 6, "open-logs": 8 }
        if (map[action] !== undefined)
            go(map[action])
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppSidebar {
            Layout.fillHeight: true
            pages: window.pages
            currentIndex: App.currentPage
            collapsed: App.sidebarCollapsed || window.width < 1100
            onNavigate: (index) => window.go(index)
            onToggle: App.sidebarCollapsed = !App.sidebarCollapsed
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            InlineMessage {
                visible: !App.helperAvailable
                severity: "warning"
                title: qsTr("Part of this program is missing")
                text: qsTr("You can look at everything, but you cannot change anything until Ubuntu RDP Manager is installed completely. "
                           + "Please reinstall it.")
                Layout.margins: Theme.m
                radius: Theme.radiusM
            }

            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: App.currentPage

                // Screens are created on first visit and then kept, so state survives navigation.
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: DashboardScreen { onOpenPage: (i) => window.go(i) } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: InstallationScreen { confirm: confirmDialog; onOpenPage: (i) => window.go(i) } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: ServerControlScreen { confirm: confirmDialog } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: SessionsScreen { confirm: confirmDialog } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: UsersScreen { confirm: confirmDialog } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: SessionLimitsScreen { confirm: confirmDialog } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: ConfigurationScreen { confirm: confirmDialog } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: PerformanceScreen {} }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: LogsScreen {} }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: DiagnosticsScreen { onOpenAction: (a) => window.openAction(a) } }
                Loader { active: StackLayout.isCurrentItem || item !== null; sourceComponent: AboutScreen {} }
            }
        }
    }

    ConfirmDialog { id: confirmDialog }

    ToastHost {
        id: toasts
        anchors.fill: parent
        z: 100
    }

    Connections {
        target: App
        function onNotify(level, text) { toasts.show(level, text) }
    }

    // Keyboard navigation: Ctrl+1..9 / Ctrl+0 jump to pages, Ctrl+PgUp/PgDown cycle.
    Repeater {
        model: 10
        delegate: Item {
            required property int index
            Shortcut {
                sequence: "Ctrl+" + ((parent.index + 1) % 10)
                onActivated: window.go(parent.index)
            }
        }
    }
    Shortcut { sequence: "Ctrl+PgDown"; onActivated: window.go(App.currentPage + 1) }
    Shortcut { sequence: "Ctrl+PgUp"; onActivated: window.go(App.currentPage - 1) }
}
