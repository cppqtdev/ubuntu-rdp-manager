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

        KeyValue {
            key: qsTr("File")
            value: page.config.certificatePath + (tlsCard.info.resolvedPath && tlsCard.info.resolvedPath !== page.config.certificatePath ? "  →  " + tlsCard.info.resolvedPath : "")
            selectable: true
        }
        KeyValue {
            key: qsTr("Secret key file")
            value: page.config.keyPath
            selectable: true
        }
        KeyValue {
            visible: !tlsCard.info.error
            key: qsTr("Made for")
            value: tlsCard.info.subject || ""
        }
        KeyValue {
            visible: !tlsCard.info.error
            key: qsTr("Made by")
            value: tlsCard.info.selfSigned ? qsTr("this computer itself (normal)") : (tlsCard.info.issuer || "")
        }
        KeyValue {
            visible: !tlsCard.info.error
            key: qsTr("Valid until")
            value: (tlsCard.info.notAfter || "") + (tlsCard.info.daysLeft !== undefined ? qsTr("  (%1 days)").arg(tlsCard.info.daysLeft) : "")
        }
        KeyValue {
            visible: !tlsCard.info.error
            key: qsTr("Fingerprint (to compare)")
            value: tlsCard.info.sha256 || ""
            selectable: true
        }
        InlineMessage {
            visible: !!tlsCard.info.error
            severity: "error"
            text: tlsCard.info.error || ""
        }

        RowLayout {
            spacing: Theme.m
            Label {
                text: qsTr("Computer name")
            }
            TextField {
                id: cnField
                text: page.config.hostName
                Layout.preferredWidth: 240
                Accessible.name: qsTr("Certificate host name")
            }
            Label {
                text: qsTr("Valid for")
            }
            SpinBox {
                id: daysField
                from: 30
                to: 825
                value: 365
                editable: true
            }
            Label {
                text: qsTr("days")
                color: Theme.textSecondary
            }
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
            Button {
                text: qsTr("Use my own: choose certificate…")
                onClicked: certDialog.open()
            }
            Label { text: certDialog.selectedFile.toString() !== "" ? certDialog.selectedFile.toString().replace("file://", "") : qsTr("none"); color: Theme.textSecondary; elide: Text.ElideMiddle; Layout.maximumWidth: 260 }
            Button {
                text: qsTr("Choose key…")
                onClicked: keyDialog.open()
            }
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
        FileDialog {
            id: certDialog
            title: qsTr("Choose PEM certificate")
            nameFilters: [qsTr("PEM files (*.pem *.crt *.cer)"), qsTr("All files (*)")]
        }
        FileDialog {
            id: keyDialog
            title: qsTr("Choose PEM private key")
            nameFilters: [qsTr("PEM files (*.pem *.key)"), qsTr("All files (*)")]
        }
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

    // ---- client features -----------------------------------------------------------------
    // Every option a remote desktop program (FreeRDP, Windows, thin clients) can ask for, with what
    // this server does about it. Options xrdp 0.9 cannot provide are listed, not hidden, so people
    // know to switch them off on the connecting computer.
    Card {
        id: featureCard
        Layout.fillWidth: true
        title: qsTr("What the connecting computer can use")
        subtitle: qsTr("Options people can switch on in their remote desktop program, and whether this computer allows them. \"Turn on\" marks the needed settings below; then press Save.")
        iconName: "link"
        visible: page.search === "" || "sound microphone audio disk usb drive printer camera webcam clipboard monitor smart card scanner nla gateway remoteapp codec h.264 rfx".indexOf(page.search) >= 0

        property int revision: 0
        Connections {
            target: page.config
            function onLoaded() { featureCard.revision++ }
            function onPendingChanged() { featureCard.revision++ }
        }
        function on(id) {
            featureCard.revision
            const v = page.config.isModified(id) ? page.config.pendingValue(id) : page.config.value(id)
            return ["1", "true", "yes"].indexOf(String(v).trim().toLowerCase()) >= 0
        }
        function componentInstalled(id) {
            const c = App.installer.components
            for (let i = 0; i < c.length; ++i)
                if (c[i].id === id)
                    return c[i].installed
            return false
        }

        readonly property var features: [
            { name: qsTr("Sound"), option: "/sound", ids: ["redirect.allChannels", "redirect.audio"], needs: "audio",
              note: qsTr("Plays on the person's speakers. If sound stutters or lags, see \"Sound tips\" below.") },
            { name: qsTr("Microphone"), option: "/microphone", ids: ["redirect.allChannels", "redirect.audio", "redirect.dynamicChannels"], needs: "audio",
              note: qsTr("The person's microphone works in the remote desktop (calls, recordings).") },
            { name: qsTr("Disks and USB sticks"), option: "/drive, /drive:hotplug", ids: ["redirect.allChannels", "redirect.drives", "redirect.driveMount"],
              note: qsTr("Appear in the folder \"%1\" in the person's home folder.").arg(page.config.value("redirect.driveFolder")) },
            { name: qsTr("Copy and paste"), option: "/clipboard", ids: ["redirect.allChannels", "redirect.clipboard"],
              note: qsTr("Text, pictures and files.") },
            { name: qsTr("Smart card readers"), option: "/smartcard", ids: ["redirect.allChannels", "redirect.drives"],
              note: qsTr("Uses the same sharing channel as disks.") },
            { name: qsTr("Several monitors"), option: "/multimon", ids: ["display.multiMonitor"], note: "" },
            { name: qsTr("Resize with the window"), option: "/dynamic-resolution", ids: ["redirect.allChannels", "redirect.dynamicChannels"], note: "" },
            { name: qsTr("Single programs (RemoteApp)"), option: "/app", ids: ["redirect.allChannels", "redirect.remoteApps"],
              note: qsTr("Basic support only.") },
            { name: qsTr("Printers"), option: "/printer", unsupported: true,
              note: qsTr("This server cannot share printers. Save as PDF into the shared disk folder and print it on the person's computer.") },
            { name: qsTr("USB devices (scanners and others)"), option: "/usb", unsupported: true,
              note: qsTr("This server cannot pass whole USB devices through. USB sticks still work through \"Disks and USB sticks\".") },
            { name: qsTr("Camera / webcam"), option: "/dvc:rdpecam", unsupported: true,
              note: qsTr("This server cannot use the person's camera.") },
            { name: qsTr("Faster graphics (RemoteFX / H.264)"), option: "/gfx, /gfx:AVC444", unsupported: true, harmless: true,
              note: qsTr("Not available here; the remote desktop program automatically uses the standard picture mode. Nothing to change.") },
            { name: qsTr("Network Level Authentication (NLA)"), option: "/sec:nla", unsupported: true,
              note: qsTr("Not available here. Switch NLA off in the remote desktop program (/sec:nla:off), otherwise it cannot connect. The connection is still encrypted.") },
            { name: qsTr("Remote Desktop Gateway"), option: "/g", unsupported: true,
              note: qsTr("Not available here. Connect directly inside your network, or through a VPN.") }
        ]

        Repeater {
            model: featureCard.features
            delegate: RowLayout {
                id: featureRow
                required property var modelData
                readonly property bool supported: !modelData.unsupported
                readonly property bool settingsOn: supported && modelData.ids.every(id => featureCard.on(id))
                readonly property bool partMissing: supported && modelData.needs !== undefined && !featureCard.componentInstalled(modelData.needs)
                Layout.fillWidth: true
                spacing: Theme.m

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        text: featureRow.modelData.name
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                    Label {
                        text: featureRow.partMissing
                              ? qsTr("Needs the \"Sound and microphone\" part — install it on the Setup page.")
                              : featureRow.modelData.note
                        visible: text !== ""
                        color: featureRow.partMissing ? Theme.warning : Theme.textSecondary
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                    Label {
                        text: qsTr("Option on the connecting computer: %1").arg(featureRow.modelData.option)
                        color: Theme.textTertiary
                        font.pixelSize: Theme.caption
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                }
                StatusPill {
                    status: !featureRow.supported ? (featureRow.modelData.harmless ? "neutral" : "warn")
                          : featureRow.partMissing ? "warn" : featureRow.settingsOn ? "pass" : "neutral"
                    text: !featureRow.supported ? qsTr("Not supported by this server")
                        : featureRow.partMissing ? qsTr("Part missing") : featureRow.settingsOn ? qsTr("Allowed") : qsTr("Off")
                }
                Button {
                    text: qsTr("Turn on")
                    visible: featureRow.supported && !featureRow.settingsOn
                    enabled: page.canAct
                    onClicked: featureRow.modelData.ids.forEach(id => { if (!featureCard.on(id)) page.config.setPending(id, "true") })
                }
                Button {
                    text: qsTr("Setup")
                    visible: featureRow.partMissing
                    onClicked: App.currentPage = 1
                }
            }
        }

        DisclosureCard {
            Layout.fillWidth: true
            title: qsTr("Sound tips (if music stutters, lags or keeps playing)")
            iconName: "info"
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                text: qsTr("• Use a separate account for remote work. If the same person is also signed in at this computer, both desktops share one sound system: sound from the local desktop is sent to the remote screen and timing gets out of step, which makes music choppy.\n"
                           + "• On the connecting computer, use a small sound buffer. For FreeRDP: /sound:sys:pulse (or /sound:sys:alsa,latency:150). A large value such as latency:700 adds almost a second of delay, and sound keeps playing for a while after the program is closed.\n"
                           + "• Remote sound needs a steady network. Use a cable instead of Wi-Fi where possible.\n"
                           + "• After changing anything, sign out of the remote desktop and connect again.")
            }
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
                Label {
                    text: modelData.created
                    color: Theme.textPrimary
                    Layout.preferredWidth: 170
                }
                Label {
                    text: modelData.file === "xrdp.ini" ? qsTr("connection settings") : qsTr("desktop & user settings")
                    color: Theme.textSecondary
                    Layout.preferredWidth: 180
                }
                Item {
                    Layout.fillWidth: true
                }
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
