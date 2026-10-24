pragma Singleton
import QtQuick

// Everyday words for internal state names, so every screen speaks the same plain language.
QtObject {
    function sessionState(s) {
        switch (s) {
        case "Connected": return qsTr("Connected")
        case "Disconnected": return qsTr("Away – still open")
        case "Orphaned": return qsTr("Stuck – can't reconnect")
        case "Closing": return qsTr("Closing…")
        default: return s
        }
    }
    function sessionStateHelp(s) {
        switch (s) {
        case "Connected": return qsTr("This person is using the remote desktop right now.")
        case "Disconnected": return qsTr("The window was closed or the network dropped. Their programs are still open and they can connect again.")
        case "Orphaned": return qsTr("Remote Desktop was restarted while this desktop was open. It still uses memory but cannot be reached any more. You can close it.")
        default: return ""
        }
    }
    function logCategory(c) {
        switch (c) {
        case "auth-failure": return qsTr("Wrong password")
        case "access-denied": return qsTr("Not allowed")
        case "limit-denied": return qsTr("Too many people")
        case "session-start": return qsTr("Signed in")
        case "session-end": return qsTr("Desktop closed")
        case "session-failure": return qsTr("Desktop failed")
        case "connection": return qsTr("Connection")
        case "tls-problem": return qsTr("Encryption problem")
        case "service": return qsTr("Server on/off")
        case "error": return qsTr("Error")
        case "info": return qsTr("Information")
        default: return c
        }
    }
    function checkStatus(s) {
        switch (s) {
        case "pass": return qsTr("OK")
        case "warn": return qsTr("Attention")
        case "fail": return qsTr("Problem")
        case "manual": return qsTr("Check yourself")
        case "skipped": return qsTr("Skipped")
        case "info": return qsTr("Note")
        default: return s
        }
    }
    function severity(s) {
        switch (s) {
        case "error": return qsTr("Error")
        case "warning": return qsTr("Warning")
        case "info": return qsTr("Info")
        default: return qsTr("Detail")
        }
    }
    function serverHeadline(plainStatus) {
        switch (plainStatus) {
        case "on": return qsTr("Remote Desktop is ON")
        case "off": return qsTr("Remote Desktop is OFF")
        case "starting": return qsTr("Remote Desktop is turning on…")
        case "stopping": return qsTr("Remote Desktop is turning off…")
        case "problem": return qsTr("Remote Desktop has a problem")
        case "not-installed": return qsTr("Remote Desktop is not set up yet")
        default: return qsTr("Checking Remote Desktop…")
        }
    }
    function serverExplanation(plainStatus) {
        switch (plainStatus) {
        case "on": return qsTr("Other computers can connect to this one.")
        case "off": return qsTr("Nobody can connect to this computer.")
        case "problem": return qsTr("It tried to start but stopped. Try turning it on again, or open the Health Check.")
        case "not-installed": return qsTr("Open Setup to install it. It takes a few minutes.")
        default: return ""
        }
    }
    function statusKey(plainStatus) {
        switch (plainStatus) {
        case "on": return "ok"
        case "problem": return "error"
        case "starting": case "stopping": return "warning"
        default: return "neutral"
        }
    }
}
