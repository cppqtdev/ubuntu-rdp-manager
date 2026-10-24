import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page

    readonly property SystemMonitor monitor: App.monitor
    readonly property LogManager logs: App.logs
    readonly property var lastTest: App.diagnostics.testHistory.length > 0 ? App.diagnostics.testHistory[0] : null

    function stageMs(id) {
        if (!lastTest || !lastTest.stages)
            return -1
        for (let i = 0; i < lastTest.stages.length; ++i)
            if (lastTest.stages[i].id === id)
                return lastTest.stages[i].ms
        return -1
    }

    title: qsTr("Performance")
    subtitle: qsTr("How hard this computer is working, updated every %1 seconds.").arg(monitor.intervalMs / 1000)

    actions: [
        ComboBox {
            model: [qsTr("Update every 2 seconds"), qsTr("Update every 5 seconds"), qsTr("Update every 10 seconds")]
            currentIndex: page.monitor.intervalMs >= 10000 ? 2 : page.monitor.intervalMs >= 5000 ? 1 : 0
            onActivated: (i) => page.monitor.intervalMs = [2000, 5000, 10000][i]
            Accessible.name: qsTr("How often to update")
        }
    ]

    InlineMessage {
        visible: page.monitor.advisorLevel !== "ok"
        severity: page.monitor.advisorLevel === "critical" ? "error" : "warning"
        title: page.monitor.advisorLevel === "critical" ? qsTr("This computer is overloaded") : qsTr("This computer is getting busy")
        text: page.monitor.advisorMessages.join("\n")
    }

    GridLayout {
        Layout.fillWidth: true
        columns: page.width >= 1000 ? 3 : 1
        columnSpacing: Theme.l
        rowSpacing: Theme.l

        Card {
            title: qsTr("Processor")
            subtitle: qsTr("%1 % busy right now (%2 cores)").arg(page.monitor.cpuPercent.toFixed(0)).arg(page.monitor.cpuCount)
            iconName: "cpu"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Sparkline {
                values: page.monitor.cpuHistory
                maxValue: 100
                Layout.fillWidth: true
                Layout.preferredHeight: 90
            }
        }
        Card {
            title: qsTr("Memory")
            subtitle: qsTr("%1 % in use · %2 free").arg(page.monitor.memUsedPercent.toFixed(0)).arg(Theme.formatMiB(page.monitor.memAvailableMiB))
            iconName: "memory"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Sparkline {
                values: page.monitor.memHistory
                maxValue: 100
                lineColor: Theme.success
                Layout.fillWidth: true
                Layout.preferredHeight: 90
            }
        }
        Card {
            title: qsTr("Open desktops")
            subtitle: qsTr("%1 right now").arg(App.sessions.totalCount)
            iconName: "sessions"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Sparkline {
                values: page.monitor.sessionHistory
                maxValue: Math.max(4, Math.max.apply(null, page.monitor.sessionHistory.length ? page.monitor.sessionHistory : [0]) + 1)
                lineColor: Theme.warning
                Layout.fillWidth: true
                Layout.preferredHeight: 90
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.l

        Card {
            title: qsTr("Details for IT staff")
            iconName: "power"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            KeyValue {
                key: qsTr("xrdp processes")
                value: qsTr("%1 (xrdp, sesman, Xorg, chansrv)").arg(page.monitor.xrdpProcessCount)
            }
            KeyValue {
                key: qsTr("Xorg servers")
                value: String(page.monitor.xorgCount)
            }
            KeyValue {
                key: qsTr("Resident memory")
                value: Theme.formatMiB(page.monitor.xrdpMemoryMiB) + qsTr(" (RSS, shared pages counted per process)")
            }
            KeyValue {
                key: qsTr("CPU")
                value: page.monitor.xrdpCpuPercent.toFixed(1) + qsTr(" % of machine")
            }
            KeyValue {
                key: qsTr("All RDP session cgroups")
                value: Theme.formatMiB(page.monitor.sessionMemoryMiB)
            }
            KeyValue {
                key: qsTr("Memory pressure (PSI)")
                value: page.monitor.memoryPressure < 0 ? qsTr("unavailable on this kernel") : page.monitor.memoryPressure.toFixed(1) + qsTr(" % (some, avg10)")
            }
            KeyValue {
                key: qsTr("Disk free (/)")
                value: page.monitor.diskFreeGiB < 0 ? qsTr("unavailable") : qsTr("%1 GiB of %2 GiB").arg(page.monitor.diskFreeGiB.toFixed(1)).arg(page.monitor.diskTotalGiB.toFixed(0))
            }
            KeyValue {
                key: qsTr("GPU")
                value: qsTr("not measured (xorgxrdp renders in software)")
            }
            KeyValue {
                key: qsTr("Per-session network")
                value: qsTr("not available (no per-session accounting in xrdp 0.9)")
            }
        }

        Card {
            title: qsTr("Sign-ins in the last 24 hours")
            iconName: "performance"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            KeyValue {
                key: qsTr("Desktops opened")
                value: String(page.logs.sessionStarts24h)
            }
            KeyValue {
                key: qsTr("Desktops that failed to open")
                value: String(page.logs.sessionFailures24h)
            }
            KeyValue {
                key: qsTr("Turned away (too many people)")
                value: String(page.logs.limitDenied24h)
            }
            KeyValue {
                key: qsTr("Wrong passwords")
                value: String(page.logs.authFailures24h)
            }
            KeyValue {
                key: qsTr("Worked first time")
                value: (page.logs.sessionStarts24h + page.logs.sessionFailures24h + page.logs.limitDenied24h) === 0 ? qsTr("nobody signed in")
                       : (100 * page.logs.sessionStarts24h / (page.logs.sessionStarts24h + page.logs.sessionFailures24h + page.logs.limitDenied24h)).toFixed(0) + " %"
            }
            KeyValue {
                key: qsTr("Last connection test")
                value: page.lastTest ? page.lastTest.time.replace("T", " ") : qsTr("not done yet (Health Check)")
            }
            KeyValue {
                key: qsTr("Time to reach the computer")
                value: page.stageMs("tcp") >= 0 ? page.stageMs("tcp") + " ms" : "—"
            }
            KeyValue {
                key: qsTr("Time for the server to answer")
                value: page.stageMs("negotiation") >= 0 ? page.stageMs("negotiation") + " ms" : "—"
            }
            KeyValue {
                key: qsTr("Time to set up encryption")
                value: page.stageMs("tls") >= 0 ? page.stageMs("tls") + " ms" : "—"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.textTertiary
                font.pixelSize: Theme.caption
                text: qsTr("These numbers are counted from the activity log.")
            }
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Memory used by each person")
        iconName: "sessions"
        EmptyState {
            visible: App.sessions.totalCount === 0
            iconName: "sessions"
            title: qsTr("Nobody is connected")
        }
        Repeater {
            model: App.sessions.model
            delegate: RowLayout {
                required property string userName
                required property string display
                required property string sessionState
                required property real memoryMiB
                required property real cpuPercent
                required property int processCount
                Layout.fillWidth: true
                spacing: Theme.m
                Label {
                    text: userName
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Layout.preferredWidth: 140
                }
                Label {
                    text: display
                    color: Theme.textSecondary
                    Layout.preferredWidth: 50
                }
                StatusPill {
                    status: sessionState
                    text: Words.sessionState(sessionState)
                }
                ProgressBar {
                    from: 0
                    to: Math.max(1, page.monitor.memTotalMiB)
                    value: Math.max(0, memoryMiB)
                    Layout.fillWidth: true
                }
                Label {
                    text: Theme.formatMiB(memoryMiB)
                    color: Theme.textPrimary
                    Layout.preferredWidth: 90
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    text: cpuPercent < 0 ? "—" : qsTr("%1 % busy").arg(cpuPercent.toFixed(1))
                    color: Theme.textSecondary
                    Layout.preferredWidth: 90
                    horizontalAlignment: Text.AlignRight
                }
                Label {
                    text: processCount < 0 ? "—" : qsTr("%1 programs").arg(processCount)
                    color: Theme.textTertiary
                    Layout.preferredWidth: 70
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }
}
