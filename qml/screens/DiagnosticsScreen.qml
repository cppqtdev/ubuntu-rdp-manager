import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Dialogs
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    signal openAction(string action)

    readonly property DiagnosticManager diag: App.diagnostics

    title: qsTr("Health Check")
    subtitle: diag.failCount === 0 && diag.warnCount === 0 ? qsTr("Everything looks fine.")
              : qsTr("Problems: %1 · Need attention: %2. Each item explains what to do.").arg(diag.failCount).arg(diag.warnCount)

    actions: [
        Button {
            text: qsTr("Save report for support…")
            icon.source: Theme.icon("export")
            enabled: !page.diag.exporting
            onClicked: bundleDialog.open()
        },
        Button {
            text: qsTr("Check again")
            icon.source: Theme.icon("refresh")
            onClicked: page.diag.runChecks()
        }
    ]

    // ---- connection test ----------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Test the connection")
        subtitle: qsTr("Checks step by step that another computer could reach this one and connect safely.")
        iconName: "diagnostics"

        RowLayout {
            spacing: Theme.m
            Label { text: qsTr("Computer") }
            TextField { id: hostField; text: App.config.hostName; Layout.preferredWidth: 240; Accessible.name: qsTr("Computer to test") }
            Label { text: qsTr("Port") }
            SpinBox { id: portField; from: 1; to: 65535; value: App.config.primaryPort; editable: true
                      textFromValue: (v) => String(v); valueFromText: (t) => parseInt(t) }
            Button {
                text: page.diag.testRunning ? qsTr("Testing…") : qsTr("Start test")
                highlighted: true
                enabled: !page.diag.testRunning
                onClicked: page.diag.runConnectionTest(hostField.text, portField.value)
            }
            Button {
                text: qsTr("Try it (opens a remote desktop window)")
                icon.source: Theme.icon("sessions")
                enabled: page.diag.availableClient !== ""
                onClicked: page.diag.launchClient(hostField.text, portField.value)
                ToolTip.visible: hovered
                ToolTip.text: page.diag.availableClient !== "" ? qsTr("Opens a remote desktop window to this computer so you can try signing in")
                                                              : qsTr("Install \"Test program\" on the Setup page first")
            }
        }
        Label {
            visible: page.diag.testSummary !== ""
            text: page.diag.testSummary
            font.weight: Font.DemiBold
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Repeater {
            model: page.diag.testStages
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.m
                StatusPill { status: modelData.status; text: Words.checkStatus(modelData.status); Layout.preferredWidth: 120 }
                Label { text: modelData.name; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.preferredWidth: 190 }
                Label { text: modelData.detail; color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Label { text: modelData.ms >= 0 ? modelData.ms + " ms" : ""; color: Theme.textTertiary; Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight }
            }
        }
        RowLayout {
            visible: page.diag.testHistory.length > 0
            spacing: Theme.m
            Label { text: qsTr("After trying it yourself, tick what worked:"); color: Theme.textSecondary }
            CheckBox { id: authOk; text: qsTr("I could sign in") }
            CheckBox { id: sessOk; text: qsTr("A desktop opened") }
            CheckBox { id: deskOk; text: qsTr("I could see and use it") }
            TextField { id: noteField; placeholderText: qsTr("Note (optional)"); Layout.fillWidth: true }
            Button {
                text: qsTr("Save result")
                onClicked: page.diag.recordManualResult(authOk.checked, sessOk.checked, deskOk.checked, noteField.text)
            }
        }
    }

    // ---- checks -------------------------------------------------------------------------
    Repeater {
        model: ["Software", "Remote desktop server", "Security", "Network", "Users and capacity", "Problems in the last 24 hours"]
        delegate: Card {
            id: groupCard
            required property string modelData
            readonly property var items: page.diag.checks.filter(c => c.group === modelData)
            visible: items.length > 0
            Layout.fillWidth: true
            title: modelData

            Repeater {
                model: groupCard.items
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.m
                    IconImage {
                        source: Theme.icon(modelData.status === "pass" ? "check" : modelData.status === "fail" ? "error"
                                           : modelData.status === "warn" ? "warning" : "info")
                        sourceSize: Qt.size(18, 18)
                        color: Theme.statusColor(modelData.status)
                        Layout.alignment: Qt.AlignTop
                        Accessible.name: modelData.status
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: modelData.title; font.weight: Font.DemiBold; color: Theme.textPrimary }
                        Label { text: modelData.detail; color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Label { visible: modelData.remedy !== ""; text: qsTr("What to do: %1").arg(modelData.remedy); color: Theme.accent; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                    Button {
                        visible: modelData.action !== ""
                        text: qsTr("Fix it")
                        flat: true
                        onClicked: page.openAction(modelData.action)
                    }
                }
            }
        }
    }

    // ---- history ------------------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Earlier tests")
        iconName: "logs"
        visible: page.diag.testHistory.length > 0
        Repeater {
            model: page.diag.testHistory.slice(0, 10)
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.m
                Label { text: String(modelData.time).replace("T", " "); color: Theme.textTertiary; Layout.preferredWidth: 170 }
                Label { text: modelData.target; color: Theme.textPrimary; Layout.preferredWidth: 200; elide: Text.ElideRight }
                Label { text: modelData.summary; color: Theme.textSecondary; elide: Text.ElideRight; Layout.fillWidth: true }
                Label {
                    text: modelData.manual ? qsTr("you tried: sign-in %1 · desktop %2 · usable %3").arg(modelData.manual.authenticated ? "✓" : "✗")
                                             .arg(modelData.manual.sessionCreated ? "✓" : "✗").arg(modelData.manual.desktopRendered ? "✓" : "✗") : ""
                    color: Theme.textSecondary
                }
            }
        }
    }

    FileDialog {
        id: bundleDialog
        title: qsTr("Save report for support")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "tar.gz"
        nameFilters: [qsTr("Report file (*.tar.gz)")]
        onAccepted: page.diag.exportBundle(selectedFile.toString())
    }
}
