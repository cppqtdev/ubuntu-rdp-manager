import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    property ConfirmDialog confirm

    readonly property RdpConfigurationManager config: App.config
    readonly property bool canAct: App.helperAvailable && config.configPresent && !config.applying
    property string search: ""

    title: qsTr("Settings")
    subtitle: qsTr("Change how Remote Desktop works. Every time you save, the old settings are kept, so you can undo it at the bottom of this page.")

    actions: [
        TextField {
            placeholderText: qsTr("Find a setting…")
            Layout.preferredWidth: 220
            onTextChanged: page.search = text.toLowerCase()
            Accessible.name: qsTr("Search settings")
        },
        Button {
            text: qsTr("Undo changes")
            enabled: page.config.pendingCount > 0
            onClicked: page.config.discard()
        },
        Button {
            text: page.config.pendingCount > 0 ? qsTr("Save changes (%1)").arg(page.config.pendingCount) : qsTr("Save")
            highlighted: true
            enabled: page.canAct && page.config.pendingCount > 0 && page.config.pendingValid
            onClicked: page.config.apply()
        }
    ]

    InlineMessage {
        visible: !page.config.configPresent
        severity: "info"
        title: qsTr("Remote Desktop is not set up yet")
        text: page.config.loadError
    }
    InlineMessage {
        visible: page.config.restartRequired
        severity: "info"
        title: qsTr("Saved — restart Remote Desktop to use it")
        text: qsTr("Go to Turn On / Off and press Restart, preferably when nobody is connected.")
    }
    InlineMessage {
        visible: !page.config.pendingValid
        severity: "error"
        title: qsTr("Some values are not valid")
        text: qsTr("Please correct the settings marked in red before saving.")
    }

    // ---- TLS ---------------------------------------------------------------------------
    Card {
        id: tlsCard
        Layout.fillWidth: true
        title: qsTr("Security certificate")
        subtitle: qsTr("This is how other computers make sure they reached the real computer and not an impostor. Connections are always encrypted with it.")
        iconName: "key"
        visible: page.search === "" || "tls certificate key".indexOf(page.search) >= 0
        readonly property var info: page.config.certificateInfo

        KeyValue { key: qsTr("File"); value: page.config.certificatePath + (tlsCard.info.resolvedPath && tlsCard.info.resolvedPath !== page.config.certificatePath ? "  →  " + tlsCard.info.resolvedPath : ""); selectable: true }
        KeyValue { key: qsTr("Secret key file"); value: page.config.keyPath; selectable: true }
        KeyValue { visible: !tlsCard.info.error; key: qsTr("Made for"); value: tlsCard.info.subject || "" }
        KeyValue { visible: !tlsCard.info.error; key: qsTr("Made by"); value: tlsCard.info.selfSigned ? qsTr("this computer itself (normal)") : (tlsCard.info.issuer || "") }
        KeyValue { visible: !tlsCard.info.error; key: qsTr("Valid until"); value: (tlsCard.info.notAfter || "") + (tlsCard.info.daysLeft !== undefined ? qsTr("  (%1 days)").arg(tlsCard.info.daysLeft) : "") }
        KeyValue { visible: !tlsCard.info.error; key: qsTr("Fingerprint (to compare)"); value: tlsCard.info.sha256 || ""; selectable: true }
        InlineMessage { visible: !!tlsCard.info.error; severity: "error"; text: tlsCard.info.error || "" }

        RowLayout {
            spacing: Theme.m
            Label { text: qsTr("Computer name") }
            TextField { id: cnField; text: page.config.hostName; Layout.preferredWidth: 240; Accessible.name: qsTr("Certificate host name") }
            Label { text: qsTr("Valid for") }
            SpinBox { id: daysField; from: 30; to: 825; value: 365; editable: true }
            Label { text: qsTr("days"); color: Theme.textSecondary }
            Button {
                text: qsTr("Create new certificate")
                enabled: page.canAct
                onClicked: page.confirm.ask(qsTr("Create a new certificate?"),
                    qsTr("A new certificate for \"%1\" will replace the current one. Computers that connected before will ask once more whether to trust this computer.").arg(cnField.text),
                    qsTr("Restart Remote Desktop afterwards to start using it."),
                    qsTr("Create"), false, () => page.config.generateCertificate(cnField.text, daysField.value))
            }
        }
        RowLayout {
            spacing: Theme.m
            Button { text: qsTr("Use my own: choose certificate…"); onClicked: certDialog.open() }
            Label { text: certDialog.selectedFile.toString() !== "" ? certDialog.selectedFile.toString().replace("file://", "") : qsTr("none"); color: Theme.textSecondary; elide: Text.ElideMiddle; Layout.maximumWidth: 260 }
            Button { text: qsTr("Choose key…"); onClicked: keyDialog.open() }
            Label { text: keyDialog.selectedFile.toString() !== "" ? keyDialog.selectedFile.toString().replace("file://", "") : qsTr("none"); color: Theme.textSecondary; elide: Text.ElideMiddle; Layout.maximumWidth: 260 }
            Button {
                text: qsTr("Install")
                enabled: page.canAct && certDialog.selectedFile.toString() !== "" && keyDialog.selectedFile.toString() !== ""
                onClicked: page.config.installCertificate(certDialog.selectedFile.toString(), keyDialog.selectedFile.toString())
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textTertiary
            font.pixelSize: Theme.caption
            text: qsTr("Most people should simply press \"Create new certificate\". If your company gives you its own certificate, choose the certificate and key files instead and press Install.")
        }
        FileDialog { id: certDialog; title: qsTr("Choose PEM certificate"); nameFilters: [qsTr("PEM files (*.pem *.crt *.cer)"), qsTr("All files (*)")] }
        FileDialog { id: keyDialog; title: qsTr("Choose PEM private key"); nameFilters: [qsTr("PEM files (*.pem *.key)"), qsTr("All files (*)")] }
    }

    // ---- Desktop -----------------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("What people see when they connect")
        iconName: "desktop"
        visible: page.search === "" || "desktop xfce window manager".indexOf(page.search) >= 0

        RadioButton {
            text: qsTr("Xfce desktop (recommended — works well for many people at once)")
            checked: page.config.desktopId === "xfce"
            enabled: page.canAct
            onClicked: page.config.configureDesktop("xfce")
        }
        RadioButton {
            text: qsTr("Ubuntu's standard choice (may not work for several people at once)")
            checked: page.config.desktopId === "system-default"
            enabled: page.canAct
            onClicked: page.config.configureDesktop("system-default")
        }
        Label {
            visible: page.config.desktopId === "custom"
            text: qsTr("Something else was chosen by hand: %1").arg(page.config.desktopScript)
            color: Theme.warning
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            text: qsTr("This only changes what remote users see. The person sitting at this computer keeps their normal desktop.")
        }
    }

    // ---- generic settings --------------------------------------------------------------
    Repeater {
        model: ["Network", "Security", "Redirection", "Display", "Logging"]
        delegate: Card {
            id: categoryCard
            required property string modelData
            Layout.fillWidth: true
            title: modelData === "Network" ? qsTr("Network") : modelData === "Security" ? qsTr("Security")
                 : modelData === "Redirection" ? qsTr("Sharing between computers") : modelData === "Display" ? qsTr("Picture quality")
                 : qsTr("Activity log detail")
            iconName: modelData === "Network" ? "network" : modelData === "Security" ? "shield" : modelData === "Logging" ? "logs"
                      : modelData === "Display" ? "desktop" : "link"
            visible: visibleRows > 0
            property int visibleRows: 0

            Repeater {
                model: page.config.settings
                delegate: SettingEditor {
                    // SettingEditor already declares these properties; assign them from the model
                    // (re-declaring them as `required` would create shadow copies the editor never sees).
                    required property var model
                    readonly property string category: model.category
                    readonly property bool internalOnly: model.internalOnly
                    settingId: model.settingId
                    type: model.type
                    label: model.label
                    help: model.help
                    choices: model.choices
                    choiceLabels: model.choiceLabels
                    minimum: model.minimum
                    maximum: model.maximum
                    value: model.value
                    pendingValue: model.pendingValue
                    modified: model.modified
                    isDefault: model.isDefault
                    error: model.error
                    iniKey: model.iniKey

                    readonly property bool shown: category === categoryCard.modelData && !internalOnly
                        && (page.search === "" || label.toLowerCase().indexOf(page.search) >= 0
                            || help.toLowerCase().indexOf(page.search) >= 0 || iniKey.toLowerCase().indexOf(page.search) >= 0)
                    visible: shown
                    onShownChanged: categoryCard.visibleRows += shown ? 1 : -1
                    Component.onCompleted: if (shown) categoryCard.visibleRows += 1
                    Component.onDestruction: if (shown) categoryCard.visibleRows -= 1
                    editable: page.canAct
                    onEdited: (v) => page.config.setPending(settingId, v)
                    onReverted: page.config.revert(settingId)
                }
            }
        }
    }

    // ---- backups -----------------------------------------------------------------------
    Card {
        Layout.fillWidth: true
        title: qsTr("Undo history")
        subtitle: qsTr("Every time settings are saved, the previous version is kept here. If something stops working after a change, restore the version from before.")
        iconName: "restart"
        visible: page.search === "" || "backup restore rollback".indexOf(page.search) >= 0

        EmptyState {
            visible: page.config.backups.length === 0
            iconName: "restart"
            title: qsTr("Nothing saved yet")
            text: qsTr("Earlier versions appear here after you save a setting for the first time.")
        }
        Repeater {
            model: page.config.backups.slice(0, 12)
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.m
                Label { text: modelData.created; color: Theme.textPrimary; Layout.preferredWidth: 170 }
                Label { text: modelData.file === "xrdp.ini" ? qsTr("connection settings") : qsTr("desktop & user settings"); color: Theme.textSecondary; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Restore")
                    flat: true
                    enabled: page.canAct
                    onClicked: page.confirm.ask(qsTr("Go back to the settings from %1?").arg(modelData.created),
                        qsTr("The settings will be set back to how they were at that time."),
                        qsTr("Your current settings are kept in the history too, so you can switch back."),
                        qsTr("Restore"), false, () => page.config.restoreBackup(modelData.file, modelData.id))
                }
            }
        }
    }
}
