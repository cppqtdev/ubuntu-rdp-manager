import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UbuntuRdpManager

// Modal confirmation for destructive or disruptive operations.
Dialog {
    id: root

    property string message: ""
    property string detail: ""
    property string confirmText: qsTr("Confirm")
    property bool destructive: false
    property var onConfirmed: null

    function ask(titleText, messageText, detailText, confirmLabel, isDestructive, callback) {
        title = titleText
        message = messageText
        detail = detailText || ""
        confirmText = confirmLabel || qsTr("Confirm")
        destructive = !!isDestructive
        onConfirmed = callback
        open()
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(520, (parent ? parent.width : 520) - 64)
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape

    contentItem: ColumnLayout {
        spacing: Theme.m
        Label {
            text: root.message
            wrapMode: Text.Wrap
            color: Theme.textPrimary
            Layout.fillWidth: true
        }
        Label {
            visible: root.detail !== ""
            text: root.detail
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: Theme.caption + 1
            Layout.fillWidth: true
        }
    }

    footer: DialogButtonBox {
        Button {
            text: root.confirmText
            highlighted: true
            icon.source: root.destructive ? Theme.icon("warning") : ""
            icon.width: 16
            icon.height: 16
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        onAccepted: {
            root.close()
            if (root.onConfirmed)
                root.onConfirmed()
        }
        onRejected: root.close()
    }
}
