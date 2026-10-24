import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    property ConfirmDialog confirm
    signal openPage(int index)

    readonly property InstallationManager inst: App.installer
    readonly property RdpConfigurationManager config: App.config
    readonly property RdpServerManager server: App.server
    readonly property bool xrdpReady: config.xrdpInstalled && config.configPresent
    readonly property bool canAct: App.helperAvailable && !config.applying && !inst.installing

    function componentInstalled(id) {
        for (let i = 0; i < inst.components.length; ++i)
            if (inst.components[i].id === id)
                return inst.components[i].installed
        return false
    }
    readonly property bool tlsOk: (config.certificateInfo.error === undefined) && App.diagnostics.checks.some(c => c.id === "tlskey" && c.status !== "fail")

    title: qsTr("Setup")
    subtitle: qsTr("Go through these steps from top to bottom. Completed steps are marked with a check. Software is downloaded only from Ubuntu's official servers.")

    actions: [
        Button {
            text: qsTr("Check again")
            icon.source: Theme.icon("refresh")
            onClicked: page.inst.refresh()
        }
    ]

    component Step: Pane {
        id: step
        property int number
        property string stepTitle
        property string description
        property string status: "neutral"   // pass, warn, fail, neutral, busy
        property string statusText
        default property alias extra: extraRow.data
        Layout.fillWidth: true
        padding: Theme.m
        background: Rectangle {
            color: Theme.subtleBg
            radius: Theme.radiusM
        }
        RowLayout {
            anchors.fill: parent
            spacing: Theme.m
            Rectangle {
                width: 28
                height: 28
                radius: Theme.radiusS
                color: Theme.tint(Theme.statusColor(step.status), Theme.dark ? 0.20 : 0.12)
                Layout.alignment: Qt.AlignTop
                IconImage {
                    anchors.centerIn: parent
                    visible: step.status === "pass"
                    source: Theme.icon("check")
                    sourceSize: Qt.size(16, 16)
                    color: Theme.statusColor(step.status)
                }
                Label {
                    anchors.centerIn: parent
                    visible: step.status !== "pass"
                    text: step.number
                    color: Theme.statusColor(step.status)
                    font.pixelSize: Theme.body
                    font.weight: Font.DemiBold
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    spacing: Theme.s
                    Label {
                        text: step.stepTitle
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    StatusPill {
                        visible: step.statusText !== ""
                        status: step.status === "busy" ? "info" : step.status
                        text: step.statusText
                        busy: step.status === "busy"
                    }
                }
                Label {
                    visible: step.description !== ""
                    text: step.description
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    id: extraRow
                    spacing: Theme.s
                    Layout.topMargin: children.length > 0 ? Theme.xs : 0
                }
            }
        }
    }

    // ---- 1. system ----------------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Step 1 · Your computer")
        iconName: "info"
        Step {
            number: 1; stepTitle: qsTr("Ubuntu version")
            description: page.inst.osName
            status: page.inst.osSupport === "verified" ? "pass" : page.inst.osSupport === "untested" ? "warn" : "fail"
            statusText: page.inst.osSupport === "verified" ? qsTr("supported") : page.inst.osSupport === "untested" ? qsTr("not tested") : qsTr("not supported")
        }
        Step {
            number: 2; stepTitle: qsTr("Tested with this version")
            description: page.inst.osVersion === "24.04" ? qsTr("Yes — this program was tested on Ubuntu 24.04.")
                                                         : qsTr("This is Ubuntu %1. The program was tested on 24.04 only, so some things may behave differently.").arg(page.inst.osVersion)
            status: page.inst.osVersion === "24.04" ? "pass" : "warn"
        }
        Step {
            number: 3; stepTitle: qsTr("Computer type")
            description: page.inst.architecture
            status: page.inst.architectureSupported ? "pass" : "fail"
        }
        Step {
            number: 4; stepTitle: qsTr("Administrator rights")
            description: !App.helperAvailable ? qsTr("Part of this program is missing. Please reinstall it.")
                         : page.inst.userIsAdmin ? qsTr("You (%1) are an administrator. When you make a change, Ubuntu asks for your password.").arg(page.inst.userName)
                                                 : qsTr("You (%1) are not an administrator. Every change needs an administrator to type their password.").arg(page.inst.userName)
            status: App.helperAvailable ? (page.inst.userIsAdmin ? "pass" : "warn") : "fail"
        }
    }

    // ---- 2. components ------------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Step 2 · Install the software")
        iconName: "install"

        Repeater {
            model: page.inst.components
            delegate: Step {
                required property var modelData
                required property int index
                number: 5 + index
                stepTitle: modelData.title
                description: modelData.why
                status: modelData.installed ? "pass" : modelData.optional ? "neutral" : "fail"
                statusText: modelData.installed ? qsTr("installed") : qsTr("not installed")
                CheckBox {
                    id: pick
                    visible: !modelData.installed
                    text: qsTr("Install this")
                    checked: !modelData.optional
                    onToggled: installPanel.toggle(modelData.id, checked)
                    Component.onCompleted: if (!modelData.installed && !modelData.optional) installPanel.toggle(modelData.id, true)
                }
            }
        }

        ColumnLayout {
            id: installPanel
            Layout.fillWidth: true
            spacing: Theme.s
            property var selected: []
            function toggle(id, on) {
                const s = selected.filter(x => x !== id)
                if (on) s.push(id)
                selected = s
            }
            readonly property var missing: page.inst.components.filter(c => !c.installed && selected.indexOf(c.id) >= 0).map(c => c.id)

            RowLayout {
                spacing: Theme.m
                Button {
                    text: page.inst.installing ? qsTr("Installing… please wait") : qsTr("Install (%1)").arg(installPanel.missing.length)
                    highlighted: true
                    enabled: page.canAct && installPanel.missing.length > 0
                    onClicked: page.confirm.ask(qsTr("Install the software now?"),
                        qsTr("This downloads and installs the selected software from Ubuntu. It needs an internet connection and usually takes 2–10 minutes."),
                        qsTr("Please do not turn off the computer while it is installing."),
                        qsTr("Install"), false, () => page.inst.install(installPanel.missing))
                }
                BusyIndicator {
                    visible: page.inst.installing
                    running: visible
                    implicitWidth: 28
                    implicitHeight: 28
                }
                Label {
                    visible: page.inst.installResult !== ""
                    text: page.inst.installResult
                    color: page.inst.installSucceeded ? Theme.success : Theme.danger
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            ScrollView {
                visible: page.inst.installLog.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    font.family: "monospace"
                    font.pixelSize: Theme.caption
                    text: page.inst.installLog.join("\n")
                    onTextChanged: cursorPosition = length
                    Accessible.name: qsTr("Installation output")
                }
            }
        }
    }

    // ---- 3. configure -------------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Step 3 · Recommended settings")
        iconName: "settings"
        enabled: page.xrdpReady
        opacity: enabled ? 1 : 0.55

        Step {
            number: 9; stepTitle: qsTr("Desktop for remote users")
            description: qsTr("Give remote users the Xfce desktop, which works well for many people at once. Your own desktop does not change.")
            status: page.config.desktopId === "xfce" ? "pass" : page.componentInstalled("xfce") ? "warn" : "fail"
            statusText: page.config.desktopId === "xfce" ? qsTr("done") : qsTr("not yet")
            Button { text: qsTr("Use Xfce"); enabled: page.canAct && page.componentInstalled("xfce") && page.config.desktopId !== "xfce"
                     onClicked: page.config.configureDesktop("xfce") }
        }
        Step {
            number: 10; stepTitle: qsTr("Sign-in safety")
            description: page.config.allowRootLogin
                         ? qsTr("People sign in with their normal Ubuntu password. The all-powerful \"root\" account may currently also connect — turn that off.")
                         : qsTr("People sign in with their normal Ubuntu password. The \"root\" account cannot connect. You can also choose exactly who may connect.")
            status: !page.config.configPresent ? "neutral" : page.config.allowRootLogin ? "warn" : "pass"
            Button { text: qsTr("Block the root account"); enabled: page.canAct && page.config.allowRootLogin
                     onClicked: page.config.applyValue("security.allowRootLogin", "false") }
            Button {
                text: qsTr("Choose who can connect…")
                flat: true
                onClicked: page.openPage(4)
            }
        }
        Step {
            number: 11; stepTitle: qsTr("Encryption")
            description: qsTr("Makes every connection private. A fresh Ubuntu installation has a known problem where encryption silently does not work; press both buttons to fix it.")
            status: page.tlsOk && page.config.securityLayer === "tls" ? "pass" : page.tlsOk ? "warn" : "fail"
            statusText: page.tlsOk && page.config.securityLayer === "tls" ? qsTr("done") : page.tlsOk ? qsTr("almost") : qsTr("not working")
            Button { text: qsTr("1. Create certificate"); enabled: page.canAct
                     onClicked: page.config.generateCertificate(page.config.hostName, 365) }
            Button { text: qsTr("2. Always encrypt"); enabled: page.canAct && page.config.securityLayer !== "tls"
                     onClicked: page.config.applyValue("security.layer", "tls") }
            Button {
                text: qsTr("More settings…")
                flat: true
                onClicked: page.openPage(6)
            }
        }
        Step {
            number: 12; stepTitle: qsTr("How many people")
            description: page.config.limitDescription
            status: page.config.limitKind === "unlimited" ? "pass" : page.config.limitKind === "limited" ? "neutral" : "fail"
            Button { text: qsTr("Remove the limit"); enabled: page.canAct && page.config.limitKind !== "unlimited"
                     onClicked: page.config.applySessionLimits({ unlimited: true }) }
            Button {
                text: qsTr("Choose a limit…")
                flat: true
                onClicked: page.openPage(5)
            }
        }
    }

    // ---- 4. start & verify --------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Step 4 · Turn on and test")
        iconName: "power"
        enabled: page.xrdpReady
        opacity: enabled ? 1 : 0.55

        Step {
            number: 13; stepTitle: qsTr("Turn on Remote Desktop")
            description: Words.serverHeadline(page.server.plainStatus) + ". " + (page.server.autostart ? qsTr("It turns on by itself when the computer starts.") : qsTr("It does not turn on by itself yet."))
            status: page.server.running && page.server.autostart && !page.config.restartRequired ? "pass" : page.server.running ? "warn" : "fail"
            statusText: page.config.restartRequired ? qsTr("restart needed") : page.server.running ? qsTr("on") : qsTr("off")
            Button {
                text: qsTr("Turn on at every start")
                enabled: page.canAct && !page.server.autostart
                onClicked: page.server.setAutostart(true)
            }
            Button { text: page.server.running ? qsTr("Restart") : qsTr("Turn on"); enabled: page.canAct && !page.server.busy
                     onClicked: page.server.running ? page.server.restart() : page.server.start() }
        }
        Step {
            number: 14; stepTitle: qsTr("Ready for connections")
            description: page.server.listening ? qsTr("Other computers can now reach this one.")
                                                : qsTr("Not ready yet — turn Remote Desktop on first.")
            status: page.server.listening ? "pass" : "fail"
        }
        Step {
            number: 15; stepTitle: qsTr("Test the connection")
            description: App.diagnostics.testSummary !== "" ? App.diagnostics.testSummary : qsTr("Checks that the computer answers and that the connection is encrypted.")
            status: App.diagnostics.testRunning ? "busy" : App.diagnostics.testStages.some(s => s.id === "certificate" && s.status === "pass") ? "pass"
                    : App.diagnostics.testStages.some(s => s.status === "fail") ? "fail" : "neutral"
            Button { text: qsTr("Run test"); enabled: page.server.listening && !App.diagnostics.testRunning
                     onClicked: App.diagnostics.runConnectionTest("127.0.0.1", page.config.primaryPort) }
            Button {
                text: qsTr("See details…")
                flat: true
                onClicked: page.openPage(9)
            }
        }
        Step {
            number: 16; stepTitle: qsTr("All done?")
            description: App.diagnostics.failCount === 0 ? qsTr("Everything is ready. See the Overview page for how people connect.")
                                                         : qsTr("Problems still to fix: %1. The Health Check explains each one.").arg(App.diagnostics.failCount)
            status: App.diagnostics.failCount === 0 ? "pass" : "fail"
            Button {
                text: qsTr("Open Health Check")
                flat: true
                onClicked: page.openPage(9)
            }
        }
    }
}
