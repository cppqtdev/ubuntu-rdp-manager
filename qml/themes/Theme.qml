pragma Singleton
import QtQuick

// Design tokens layered on top of the FluentWinUI3 style. Colours follow the WinUI 3 palette
// for light and dark schemes; the active scheme comes from Application.styleHints, which the
// application controller sets from the user's preference.
QtObject {
    // Same rule FluentWinUI3 uses (Config.qml: anything but Light selects the dark assets), so the
    // controls and these tokens can never disagree.
    readonly property bool dark: Application.styleHints.colorScheme !== Qt.Light

    // surfaces
    readonly property color windowBg: dark ? "#202020" : "#F3F3F3"
    readonly property color sidebarBg: dark ? "#1A1A1A" : "#EBEBEB"
    readonly property color cardBg: dark ? "#2B2B2B" : "#FFFFFF"
    readonly property color cardBorder: dark ? "#3B3B3B" : "#E3E3E3"
    readonly property color subtleBg: dark ? "#323232" : "#F6F6F6"
    readonly property color hoverBg: dark ? "#383838" : "#EDEDED"
    readonly property color selectedBg: dark ? "#3A3A3A" : "#E1E1E1"
    readonly property color divider: dark ? "#3B3B3B" : "#E5E5E5"

    // text
    readonly property color textPrimary: dark ? "#FFFFFF" : "#1B1B1B"
    readonly property color textSecondary: dark ? "#CFCFCF" : "#5C5C5C"
    readonly property color textTertiary: dark ? "#9E9E9E" : "#8A8A8A"

    // semantic (WinUI system colours; contrast ≥ 4.5:1 on cardBg)
    readonly property color accent: dark ? "#60CDFF" : "#005FB8"
    readonly property color success: dark ? "#6CCB5F" : "#0F7B0F"
    readonly property color warning: dark ? "#FCE100" : "#9D5D00"
    readonly property color danger: dark ? "#FF99A4" : "#C42B1C"
    readonly property color neutral: dark ? "#9E9E9E" : "#6E6E6E"
    readonly property color brand: "#E95420"

    // spacing (4 px grid) and shape
    readonly property int xs: 4
    readonly property int s: 8
    readonly property int m: 12
    readonly property int l: 16
    readonly property int xl: 24
    readonly property int xxl: 32
    readonly property int radiusS: 4
    readonly property int radiusM: 8
    readonly property int radiusL: 12

    // WinUI type ramp
    readonly property int caption: 12
    readonly property int body: 14
    readonly property int subtitle: 20
    readonly property int title: 28
    readonly property int display: 36

    readonly property int sidebarWidth: 248
    readonly property int sidebarCollapsedWidth: 56
    readonly property int contentMaxWidth: 1400

    function tint(c, alpha) {
        return Qt.rgba(c.r, c.g, c.b, alpha)
    }

    // status keys used across the UI: ok/running/pass, warning/warn/starting/stopping,
    // error/failed/fail/critical, info, neutral/stopped/unknown/requires-setup
    function statusColor(key) {
        switch (key) {
        case "ok": case "running": case "pass": case "success": case "Connected":
            return success
        case "warning": case "warn": case "starting": case "stopping": case "Disconnected":
            return warning
        case "error": case "failed": case "fail": case "critical": case "denies-all": case "invalid": case "Orphaned":
            return danger
        case "info": case "manual": case "unlimited":
            return accent
        default:
            return neutral
        }
    }

    function icon(name) {
        return "qrc:/qt/qml/UbuntuRdpManager/resources/icons/" + name + ".svg"
    }

    function formatMiB(v) {
        if (v === undefined || v < 0)
            return "unavailable"
        if (v >= 1024)
            return (v / 1024).toFixed(1) + " GiB"
        return Math.round(v) + " MiB"
    }
}
