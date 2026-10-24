import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page

    readonly property LogManager logs: App.logs

    title: qsTr("Activity Log")
    subtitle: qsTr("Everything Remote Desktop recorded: who signed in, wrong passwords, problems. Newest first. Saved copies never contain passwords.")

    actions: [
        Switch {
            text: qsTr("Update live")
            checked: page.logs.follow
            onToggled: page.logs.follow = checked
        },
        Button {
            text: qsTr("Save a copy…")
            icon.source: Theme.icon("export")
            enabled: page.logs.model.count > 0
            onClicked: exportDialog.open()
        },
        Button {
            text: qsTr("Refresh")
            icon.source: Theme.icon("refresh")
            onClicked: page.logs.reload()
        }
    ]

    InlineMessage {
        visible: page.logs.needsElevation
        severity: "warning"
        title: qsTr("Your account cannot read the activity log directly")
        text: qsTr("Press the button to show it. Ubuntu will ask for an administrator password.")
        actionText: qsTr("Show log (asks for password)")
        onActionTriggered: page.logs.loadViaHelper()
    }
    InlineMessage {
        visible: page.logs.lastError !== ""
        severity: "error"
        text: page.logs.lastError
    }

    Card {
        Layout.fillWidth: true

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.m
            TextField {
                placeholderText: qsTr("Search, e.g. a user name…")
                Layout.fillWidth: true
                onTextChanged: page.logs.model.searchText = text
                Accessible.name: qsTr("Search log messages")
            }
            ComboBox {
                Layout.preferredWidth: 240
                model: [qsTr("Important events only"), qsTr("Problems only"), qsTr("Everything (technical)")]
                onActivated: (i) => page.logs.model.mode = ["important", "problems", "all"][i]
                Accessible.name: qsTr("What to show")
            }
            ComboBox {
                Layout.preferredWidth: 190
                model: [qsTr("All kinds of events")].concat(page.logs.categories.map(c => Words.logCategory(c)))
                onActivated: (i) => page.logs.model.category = i === 0 ? "" : page.logs.categories[i - 1]
                Accessible.name: qsTr("Kind of event")
            }
            Label {
                text: qsTr("showing %1 of %2").arg(page.logs.model.count).arg(page.logs.totalCount)
                color: Theme.textSecondary
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

        BusyIndicator { visible: page.logs.loading && page.logs.totalCount === 0; running: visible; Layout.alignment: Qt.AlignHCenter }
        EmptyState {
            visible: !page.logs.loading && page.logs.model.count === 0
            iconName: "logs"
            title: page.logs.totalCount === 0 ? qsTr("Nothing recorded yet") : qsTr("Nothing to show here")
            text: page.logs.totalCount > 0 && page.logs.model.mode !== "all" ? qsTr("Choose \"Everything (technical)\" to see all recorded lines.") : ""
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(320, page.height - 330)
            clip: true
            model: page.logs.model
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Log entries, newest first")

            delegate: Rectangle {
                id: entry
                required property int index
                required property string timeText
                required property string severity
                required property string source
                required property string message
                required property string category
                required property string plain

                width: ListView.view.width
                implicitHeight: row.implicitHeight + 10
                color: index % 2 ? "transparent" : Theme.subtleBg

                RowLayout {
                    id: row
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.s
                    anchors.rightMargin: Theme.s
                    spacing: Theme.m
                    Label { text: entry.timeText; color: Theme.textTertiary; font.family: "monospace"; font.pixelSize: Theme.caption + 1; Layout.preferredWidth: 110; Layout.alignment: Qt.AlignTop }
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.statusColor(entry.severity); Layout.alignment: Qt.AlignTop; Layout.topMargin: 5
                                Accessible.name: Words.severity(entry.severity) }
                    Label { text: entry.source === "xrdp-sesman" ? qsTr("desktops") : entry.source === "xrdp" ? qsTr("connections") : entry.source === "systemd" ? qsTr("system") : entry.source
                            color: Theme.textSecondary; font.pixelSize: Theme.caption + 1; Layout.preferredWidth: 90; elide: Text.ElideRight; Layout.alignment: Qt.AlignTop }
                    Label { text: Words.logCategory(entry.category); color: Theme.statusColor(entry.category === "info" || entry.category === "service" ? "neutral" : entry.category === "session-start" ? "ok" : "warning")
                            font.pixelSize: Theme.caption; Layout.preferredWidth: 130; elide: Text.ElideRight; Layout.alignment: Qt.AlignTop }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label { text: entry.plain; color: Theme.textPrimary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Label { visible: entry.plain !== entry.message; text: entry.message; color: Theme.textTertiary; wrapMode: Text.WrapAnywhere
                                font.family: "monospace"; font.pixelSize: Theme.caption; Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Save a copy of the activity log")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: [qsTr("Text files (*.txt)")]
        onAccepted: page.logs.exportFiltered(selectedFile.toString())
    }
}
