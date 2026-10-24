import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    property ConfirmDialog confirm

    readonly property RdpConfigurationManager config: App.config
    readonly property SystemMonitor monitor: App.monitor
    readonly property bool canAct: App.helperAvailable && config.configPresent && !config.applying

    // editor state, initialised from the files and re-synced after every reload
    property bool unlimited: true
    property int maxSessions: 50
    property string policy: "Default"
    property bool killDisconnected: false
    property int disconnectedMinutes: 0
    property int idleMinutes: 0

    function sync() {
        unlimited = config.limitKind === "unlimited"
        maxSessions = config.limitKind === "limited" ? config.maxSessions : 50
        policy = config.policy
        killDisconnected = config.killDisconnected
        disconnectedMinutes = Math.round(config.disconnectedTimeLimit / 60)
        idleMinutes = Math.round(config.idleTimeLimit / 60)
    }
    readonly property bool dirty: unlimited !== (config.limitKind === "unlimited")
                                  || (!unlimited && maxSessions !== config.maxSessions)
                                  || policy !== config.policy || killDisconnected !== config.killDisconnected
                                  || disconnectedMinutes * 60 !== config.disconnectedTimeLimit
                                  || idleMinutes * 60 !== config.idleTimeLimit

    Component.onCompleted: sync()
    Connections {
        target: page.config
        function onLoaded() { page.sync() }
    }

    title: qsTr("Connection Limits")
    subtitle: qsTr("Decide how many people may be connected at the same time, and what happens to desktops people leave open.")

    actions: [
        Button { text: qsTr("Undo changes"); enabled: page.dirty; onClicked: page.sync() },
        Button {
            text: qsTr("Save")
            highlighted: true
            enabled: page.canAct && page.dirty
            onClicked: page.config.applySessionLimits({
                unlimited: page.unlimited, maxSessions: page.maxSessions, policy: page.policy,
                killDisconnected: page.killDisconnected,
                disconnectedTimeLimit: page.disconnectedMinutes * 60, idleTimeLimit: page.idleMinutes * 60 })
        }
    ]

    InlineMessage {
        visible: page.config.limitKind === "denies-all"
        severity: "error"
        title: qsTr("Right now nobody can connect")
        text: qsTr("The limit is set to 0, which this remote desktop software treats as \"block everyone\". Switch on \"No limit\" below and press Save.")
    }
    InlineMessage {
        visible: page.config.restartRequired
        severity: "info"
        title: qsTr("Saved — restart Remote Desktop to use it")
        text: qsTr("Go to Turn On / Off and press Restart. Do it when nobody is connected, because open desktops cannot be reached again after a restart.")
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("How many people at the same time")
        iconName: "limits"

        Switch {
            text: qsTr("No limit")
            checked: page.unlimited
            onToggled: page.unlimited = checked
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            text: qsTr("Any number of people may connect, as long as the computer has enough memory.")
                  + "  " + (page.config.zeroMeansUnlimited
                  ? qsTr("(Technical: stored as MaxSessions=0.)")
                  : qsTr("(Technical: stored as MaxSessions=%1, because this version of the server would block everyone with 0.)").arg(page.config.unlimitedConfigValue()))
        }
        RowLayout {
            enabled: !page.unlimited
            opacity: enabled ? 1 : 0.5
            spacing: Theme.m
            Label {
                text: qsTr("At most this many people:")
            }
            SpinBox {
                from: 1
                to: 9998
                editable: true
                value: page.maxSessions
                onValueModified: page.maxSessions = value
                Accessible.name: qsTr("Maximum number of people")
            }
        }
        InlineMessage {
            severity: "info"
            title: qsTr("\"No limit\" does not mean the computer can handle any number")
            text: qsTr("Every desktop needs memory and CPU. Measured on the test systems: about 150 MiB per idle minimal Xfce session and about 300 MiB with the full xfce4 package and its desktop services; real work (browsers, office) needs far more. Watch the capacity estimate below.")
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("How many more people this computer can handle")
        iconName: "memory"
        subtitle: qsTr("A live estimate based on free memory. It changes as people work.")

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.l
            MetricTile {
                label: qsTr("Desktops open now")
                value: App.sessions.totalCount
                iconName: "sessions"
            }
            MetricTile {
                label: qsTr("Room for about")
                value: page.monitor.hasData ? qsTr("%1 more").arg(page.monitor.estimatedAdditionalSessions) : "…"
                status: page.monitor.advisorLevel
                caption: qsTr("counting %1 of memory per person").arg(Theme.formatMiB(Math.max(page.monitor.perSessionMiB, page.monitor.plannedSessionMiB)))
            }
            MetricTile { label: qsTr("Free memory"); value: Theme.formatMiB(page.monitor.memAvailableMiB); iconName: "memory"
                         caption: qsTr("of %1").arg(Theme.formatMiB(page.monitor.memTotalMiB)) }
        }
        RowLayout {
            spacing: Theme.m
            Button {
                text: qsTr("Set the limit to what fits")
                enabled: page.monitor.hasData
                onClicked: {
                    page.unlimited = false
                    page.maxSessions = Math.max(1, App.sessions.totalCount + page.monitor.estimatedAdditionalSessions)
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                text: qsTr("Sets the maximum to the people connected now plus the estimate above (%1). Check it, then press Save.")
                      .arg(App.sessions.totalCount + page.monitor.estimatedAdditionalSessions)
            }
        }
        GridLayout {
            columns: 3
            columnSpacing: Theme.m
            Label {
                text: qsTr("Memory to plan per person")
            }
            SpinBox { from: 64; to: 16384; stepSize: 64; editable: true; value: page.monitor.plannedSessionMiB
                      onValueModified: page.monitor.plannedSessionMiB = value }
            Label {
                text: qsTr("MiB")
                color: Theme.textSecondary
            }
            Label {
                text: qsTr("Warn me when free memory drops below")
            }
            SpinBox { from: 1; to: 90; editable: true; value: Math.round(page.monitor.memWarnPercent)
                      onValueModified: page.monitor.memWarnPercent = value }
            Label {
                text: "%"
                color: Theme.textSecondary
            }
            Label {
                text: qsTr("Warn me when the processor is busier than")
            }
            SpinBox { from: 1; to: 80; editable: true; value: Math.round(page.monitor.loadWarnPerCore * 10)
                      textFromValue: (v) => (v / 10).toFixed(1); valueFromText: (t) => Math.round(parseFloat(t) * 10)
                      onValueModified: page.monitor.loadWarnPerCore = value / 10 }
            Label {
                text: "×"
                color: Theme.textSecondary
            }
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("When someone connects again")
        iconName: "sessions"

        RowLayout {
            spacing: Theme.m
            Label {
                text: qsTr("They get:")
            }
            ComboBox {
                id: policyBox
                Layout.preferredWidth: 380
                textRole: "label"
                valueRole: "value"
                model: [
                    { value: "Default", label: qsTr("Their open desktop back (recommended)") },
                    { value: "UBC", label: qsTr("A new desktop every time") },
                    { value: "UBD", label: qsTr("Their desktop back if the screen size matches") },
                    { value: "UBI", label: qsTr("Their desktop back from the same computer") },
                    { value: "UBDI", label: qsTr("Desktop back: same computer and screen size") },
                    { value: "UBDC", label: qsTr("A new desktop every time (by screen size)") }
                ]
                currentIndex: Math.max(0, indexOfValue(page.policy))
                onActivated: page.policy = currentValue
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            text: page.policy === "Default"
                  ? qsTr("Every person gets their own desktop. If they disconnect and come back, they continue exactly where they left off.")
                  : page.policy === "UBC" || page.policy === "UBDC"
                    ? qsTr("Lets one person have several desktops at once. But after disconnecting they can never get back to an old desktop, so also switch on \"Close desktops that were left open\" below.")
                    : qsTr("They return to their open desktop only when the condition matches; otherwise they get a new one.")
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Desktops that are left open")
        iconName: "disconnect"

        Switch {
            text: qsTr("Close desktops that were left open")
            checked: page.killDisconnected
            onToggled: page.killDisconnected = checked
        }
        RowLayout {
            enabled: page.killDisconnected
            opacity: enabled ? 1 : 0.5
            spacing: Theme.m
            Label {
                text: qsTr("after")
            }
            SpinBox {
                from: 0
                to: 43200
                editable: true
                value: page.disconnectedMinutes
                onValueModified: page.disconnectedMinutes = value
            }
            Label {
                text: qsTr("minutes (0 = right away)")
                color: Theme.textSecondary
            }
        }
        RowLayout {
            spacing: Theme.m
            Label {
                text: qsTr("Disconnect people who do nothing for")
            }
            SpinBox {
                from: 0
                to: 43200
                editable: true
                value: page.idleMinutes
                onValueModified: page.idleMinutes = value
            }
            Label {
                text: qsTr("minutes (0 = never). Their desktop stays open.")
                color: Theme.textSecondary
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textTertiary
            font.pixelSize: Theme.caption
            text: qsTr("Closing a desktop also closes the programs in it, so unsaved work is lost.")
        }
    }
}
