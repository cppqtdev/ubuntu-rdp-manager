import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page

    title: qsTr("About & Help")
    subtitle: qsTr("Remote Desktop Manager %1").arg(App.version)

    Card {
        Layout.fillWidth: true
        title: qsTr("Common questions")
        iconName: "about"

        component Faq: ColumnLayout {
            property string question
            property string answer
            Layout.fillWidth: true
            spacing: 2
            Label { text: parent.question; font.weight: Font.DemiBold; color: Theme.textPrimary; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: parent.answer; color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.bottomMargin: Theme.s }
        }

        Faq { question: qsTr("How do people connect?")
              answer: qsTr("On the Overview page you see this computer's address. On Windows they open \"Remote Desktop Connection\", type the address and sign in with their Ubuntu user name and password.") }
        Faq { question: qsTr("Do several people get separate desktops?")
              answer: qsTr("Yes. Every person has their own desktop, programs and files. They cannot see each other's screens.") }
        Faq { question: qsTr("Why does Ubuntu ask for my password?")
              answer: qsTr("Changing Remote Desktop affects everyone, so Ubuntu checks that an administrator is making the change. This program never sees or stores your password.") }
        Faq { question: qsTr("Someone closed the window. Is their work lost?")
              answer: qsTr("No. Their desktop stays open (shown as \"Away\"). When they connect again they continue where they left off.") }
        Faq { question: qsTr("What does \"Stuck – can't reconnect\" mean?")
              answer: qsTr("Remote Desktop was restarted while that desktop was open. It still uses memory but nobody can get back to it. You can close it on the \"Who's Connected\" page.") }
        Faq { question: qsTr("The other computer warns about a certificate.")
              answer: qsTr("That is normal the first time. Compare the fingerprint it shows with the one on the Settings page. If they match, it is safe to continue.") }
        Faq { question: qsTr("Can people connect from home over the internet?")
              answer: qsTr("Not directly — that would be unsafe. Ask your IT person to set up a VPN; then it works as if they were in the office.") }
        Faq { question: qsTr("Something does not work.")
              answer: qsTr("Open the Health Check. It lists every problem it finds in plain words, with a \"Fix it\" button where possible. You can also save a report for support there.") }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Keyboard shortcuts")
        iconName: "info"
        KeyValue { key: "Ctrl+1 … Ctrl+0"; value: qsTr("Jump to page 1 … 10 in the menu") }
        KeyValue { key: "Ctrl+PgUp / Ctrl+PgDown"; value: qsTr("Previous / next page") }
        KeyValue { key: "Tab / Shift+Tab"; value: qsTr("Move between buttons and fields") }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Versions")
        iconName: "settings"
        KeyValue { key: qsTr("This program"); value: App.version }
        KeyValue { key: qsTr("Remote desktop server"); value: App.config.xrdpPackageVersion !== "" ? "xrdp " + App.config.xrdpPackageVersion : qsTr("not installed") }
        KeyValue { key: qsTr("Display support"); value: App.config.xorgxrdpPackageVersion !== "" ? "xorgxrdp " + App.config.xorgxrdpPackageVersion : qsTr("not installed") }
        KeyValue { key: qsTr("Administrator tools"); value: App.helperAvailable ? qsTr("installed") : qsTr("missing") }
        KeyValue { key: qsTr("Built with"); value: "Qt " + App.qtVersion + " (FluentWinUI3)" }
        KeyValue { key: qsTr("Documentation"); value: "/opt/ubuntu-rdp-manager/share/doc"; selectable: true }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("How it works (for IT staff)")
        iconName: "shield"
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            text: qsTr("The remote desktop protocol is provided by xrdp from Ubuntu. This program runs without special rights and reads the system state. "
                       + "Every change goes to a small separate helper that asks Ubuntu (polkit) for administrator permission, checks the request, "
                       + "keeps a copy of the old settings and records the change in /var/log/ubuntu-rdp-manager/audit.log.")
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Third-party software and licences")
        iconName: "logs"
        component Lic: RowLayout {
            property string name
            property string license
            property string note
            Layout.fillWidth: true
            spacing: Theme.m
            Label { text: parent.name; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.preferredWidth: 200 }
            Label { text: parent.license; color: Theme.textSecondary; Layout.preferredWidth: 200 }
            Label { text: parent.note; color: Theme.textTertiary; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        Lic { name: "Qt 6.8"; license: "LGPL-3.0-only"; note: qsTr("Included; the library files can be replaced. Source: https://download.qt.io") }
        Lic { name: "FluentWinUI3 style"; license: "LGPL-3.0-only"; note: qsTr("Part of Qt") }
        Lic { name: "xrdp / xorgxrdp"; license: "Apache-2.0 / MIT"; note: qsTr("Installed from Ubuntu, not included") }
        Lic { name: "FreeRDP 3 (optional)"; license: "Apache-2.0"; note: qsTr("Installed from Ubuntu, not included") }
        Lic { name: "ICU"; license: "Unicode-3.0"; note: qsTr("Included with Qt") }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textTertiary
            font.pixelSize: Theme.caption
            text: qsTr("Full licence texts: /opt/ubuntu-rdp-manager/share/doc/licenses.")
        }
    }
}
