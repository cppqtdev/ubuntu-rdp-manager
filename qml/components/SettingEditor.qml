import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

// One editable setting, rendered according to its schema type. Emits edited(value);
// validation happens in C++ (ConfigSchema) and is shown via `error`.
Pane {
    id: root

    property string settingId
    property string type
    property string label
    property string help
    property var choices: []
    property var choiceLabels: []
    property int minimum: 0
    property int maximum: 0
    property string value
    property string pendingValue
    property bool modified: false
    property bool isDefault: false
    property string error
    property string iniKey
    property bool editable: true

    signal edited(string newValue)
    signal reverted()

    function boolValue(v) {
        const s = String(v).toLowerCase()
        return s === "true" || s === "yes" || s === "1"
    }

    Layout.fillWidth: true
    padding: Theme.m
    background: Rectangle {
        radius: Theme.radiusM
        color: root.modified ? Theme.tint(Theme.accent, Theme.dark ? 0.10 : 0.06) : Theme.subtleBg
        border.color: root.error !== "" ? Theme.danger : (root.modified ? Theme.tint(Theme.accent, 0.5) : "transparent")
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.l

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 3
            spacing: 2
            RowLayout {
                spacing: Theme.s
                Label {
                    text: root.label
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Label {
                    visible: root.modified
                    text: qsTr("changed, not saved yet")
                    font.pixelSize: Theme.caption
                    color: Theme.accent
                }
                Label {
                    visible: root.isDefault && !root.modified
                    text: qsTr("standard value")
                    font.pixelSize: Theme.caption
                    color: Theme.textTertiary
                }
            }
            Label {
                text: root.help
                color: Theme.textSecondary
                font.pixelSize: Theme.caption + 1
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Technical name: %1").arg(root.iniKey)
                color: Theme.textTertiary
                font.pixelSize: Theme.caption - 1
            }
            Label {
                visible: root.error !== ""
                text: root.error
                color: Theme.danger
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        Item {
            Layout.preferredWidth: 300
            Layout.minimumWidth: 220
            implicitHeight: editorLoader.implicitHeight
            Layout.alignment: Qt.AlignVCenter

            Loader {
                id: editorLoader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.editable
                sourceComponent: {
                    switch (root.type) {
                    case "bool": return boolEditor
                    case "int": case "maxsessions": return intEditor
                    case "enum": return enumEditor
                    case "tlsprotocols": return tlsEditor
                    case "enumlist": return enumListEditor
                    default: return textEditor
                    }
                }
            }
        }

        ToolButton {
            visible: root.modified
            icon.source: Theme.icon("restart")
            icon.width: 16
            icon.height: 16
            Accessible.name: qsTr("Revert %1").arg(root.label)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Undo this change")
            onClicked: root.reverted()
        }
    }

    Component {
        id: boolEditor
        Switch {
            checked: root.boolValue(root.pendingValue)
            text: checked ? qsTr("On") : qsTr("Off")
            Accessible.name: root.label
            onToggled: root.edited(checked ? "true" : "false")
        }
    }
    Component {
        id: intEditor
        SpinBox {
            from: root.minimum
            to: root.type === "maxsessions" ? 9999 : root.maximum
            value: parseInt(root.pendingValue) || 0
            editable: true
            Accessible.name: root.label
            onValueModified: root.edited(String(value))
        }
    }
    Component {
        id: enumEditor
        ComboBox {
            model: root.choiceLabels.length === root.choices.length ? root.choiceLabels : root.choices
            Accessible.name: root.label
            currentIndex: {
                for (let i = 0; i < root.choices.length; ++i)
                    if (String(root.choices[i]).toLowerCase() === String(root.pendingValue).toLowerCase())
                        return i
                return -1
            }
            onActivated: (index) => root.edited(root.choices[index])
        }
    }
    Component {
        id: enumListEditor
        ComboBox {
            readonly property bool custom: root.choices.indexOf(String(root.pendingValue).trim().toLowerCase()) < 0
            model: (root.choiceLabels.length === root.choices.length ? root.choiceLabels : root.choices)
                   .concat(custom ? [qsTr("Custom: %1").arg(root.pendingValue)] : [])
            currentIndex: custom ? root.choices.length : root.choices.indexOf(String(root.pendingValue).trim().toLowerCase())
            Accessible.name: root.label
            onActivated: (index) => { if (index < root.choices.length) root.edited(root.choices[index]) }
        }
    }
    Component {
        id: tlsEditor
        RowLayout {
            spacing: Theme.m
            readonly property var parts: String(root.pendingValue).split(",").map(s => s.trim())
            function emitValue(v12, v13) {
                const out = []
                if (v12) out.push("TLSv1.2")
                if (v13) out.push("TLSv1.3")
                root.edited(out.join(", "))
            }
            CheckBox {
                id: tls12
                text: "TLS 1.2"
                checked: parent.parts.indexOf("TLSv1.2") >= 0
                onToggled: parent.emitValue(checked, tls13.checked)
            }
            CheckBox {
                id: tls13
                text: "TLS 1.3"
                checked: parent.parts.indexOf("TLSv1.3") >= 0
                onToggled: parent.emitValue(tls12.checked, checked)
            }
        }
    }
    Component {
        id: textEditor
        TextField {
            text: root.pendingValue
            selectByMouse: true
            placeholderText: root.type === "ciphers" ? qsTr("OpenSSL default") : ""
            font.family: root.type === "listen" || root.type === "ciphers" || root.type === "path" ? "monospace" : ""
            Accessible.name: root.label
            onEditingFinished: if (text !== root.pendingValue) root.edited(text)
        }
    }
}
