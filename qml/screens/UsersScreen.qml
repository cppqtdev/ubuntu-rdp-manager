import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import UbuntuRdpManager

PageScaffold {
    id: page
    property ConfirmDialog confirm

    readonly property UserAccessManager users: App.users
    readonly property bool canAct: App.helperAvailable && !users.busy

    title: qsTr("Who Can Connect")
    subtitle: qsTr("People sign in with the same user name and password they use on this computer. Here you decide which of them may connect remotely. This program never sees or stores passwords.")

    actions: [
        Button {
            text: qsTr("Refresh")
            icon.source: Theme.icon("refresh")
            onClicked: page.users.refresh()
        }
    ]

    InlineMessage {
        visible: page.users.everyoneLockedOut
        severity: "error"
        title: qsTr("Nobody is allowed to connect")
        text: qsTr("You chose \"Only the people I choose\", but nobody is chosen. Allow at least one person below, or switch to \"Everyone\".")
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("Who may connect remotely")
        iconName: "lock"

        RadioButton {
            id: everyone
            text: qsTr("Everyone who has an account on this computer")
            checked: !page.users.restrictionEnabled
            enabled: page.canAct
            onClicked: {
                if (!page.users.restrictionEnabled)
                    return
                page.confirm.ask(qsTr("Let everyone connect?"),
                    qsTr("Anyone who has an account and password on this computer will be able to connect remotely."),
                    qsTr("Your list of chosen people is kept, so you can switch back later."),
                    qsTr("Let everyone connect"), true, () => page.users.disableRestriction())
                checked = Qt.binding(() => !page.users.restrictionEnabled)
            }
        }
        RadioButton {
            id: restricted
            text: qsTr("Only the people I choose (recommended)")
            checked: page.users.restrictionEnabled
            enabled: page.canAct
            onClicked: {
                if (!page.users.restrictionEnabled)
                    enableDialog.open()
                checked = Qt.binding(() => page.users.restrictionEnabled)
            }
        }
        KeyValue {
            visible: page.users.restrictionEnabled
            key: qsTr("Chosen people")
            value: page.users.groupMembers.length > 0 ? page.users.groupMembers.join(", ") : qsTr("nobody yet")
        }
        Label {
            text: qsTr("Changes apply the next time someone signs in. People who are already connected are not disconnected.")
            color: Theme.textTertiary
            font.pixelSize: Theme.caption
        }
    }

    Card {
        Layout.fillWidth: true
        title: qsTr("People with an account on this computer")
        subtitle: qsTr("To add a new person, create a user account in Ubuntu Settings → System → Users.")
        iconName: "users"

        BusyIndicator {
            visible: page.users.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s
            visible: page.users.model.count > 0
            component Head: Label { color: Theme.textSecondary; font.pixelSize: Theme.caption; font.weight: Font.DemiBold }
            Head {
                text: qsTr("Person")
                Layout.preferredWidth: 220
            }
            Head {
                text: ""
                Layout.preferredWidth: 60
            }
            Head {
                text: qsTr("Desktops open")
                Layout.preferredWidth: 100
            }
            Head {
                text: qsTr("Recent sign-in problem")
                Layout.fillWidth: true
            }
            Head {
                text: qsTr("May connect")
                Layout.preferredWidth: 170
                horizontalAlignment: Text.AlignRight
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
            visible: page.users.model.count > 0
        }

        EmptyState {
            visible: !page.users.loading && page.users.model.count === 0
            iconName: "users"
            title: qsTr("No user accounts found")
        }

        Repeater {
            model: page.users.model
            delegate: RowLayout {
                id: userRow
                required property string name
                required property int uid
                required property string fullName
                required property bool isAdmin
                required property bool rdpAllowed
                required property int sessionCount
                required property string lastDenied

                Layout.fillWidth: true
                Layout.preferredHeight: 44
                spacing: Theme.s

                RowLayout {
                    Layout.preferredWidth: 220
                    Layout.fillWidth: false
                    spacing: Theme.s
                    IconImage {
                        source: Theme.icon("user")
                        sourceSize: Qt.size(18, 18)
                        color: Theme.textSecondary
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: userRow.name + (userRow.isAdmin ? "  ·  " + qsTr("administrator") : "")
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: userRow.fullName !== ""
                            text: userRow.fullName
                            color: Theme.textTertiary
                            font.pixelSize: Theme.caption
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
                Item {
                    Layout.preferredWidth: 60
                }
                Label {
                    text: userRow.sessionCount
                    color: userRow.sessionCount > 0 ? Theme.success : Theme.textTertiary
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 100
                }
                Label { text: userRow.lastDenied ? userRow.lastDenied.replace("auth-failure", qsTr("wrong password")).replace("access-denied", qsTr("not allowed")).replace("limit-denied", qsTr("too many people")) : "—"
                    color: userRow.lastDenied ? Theme.warning : Theme.textTertiary; elide: Text.ElideRight; Layout.fillWidth: true }
                Item {
                    Layout.preferredWidth: 170
                    implicitHeight: sw.implicitHeight
                    Switch {
                        id: sw
                        anchors.right: parent.right
                        text: userRow.rdpAllowed ? qsTr("Yes") : qsTr("No")
                        checked: userRow.rdpAllowed
                        enabled: page.canAct && page.users.restrictionEnabled
                        Accessible.name: qsTr("Remote desktop access for %1").arg(userRow.name)
                        ToolTip.visible: hovered && !page.users.restrictionEnabled
                        ToolTip.text: qsTr("Everyone may connect. Choose \"Only the people I choose\" above to decide person by person.")
                        onToggled: {
                            page.users.setUserAllowed(userRow.name, checked)
                            checked = Qt.binding(() => userRow.rdpAllowed)
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: enableDialog
        title: qsTr("Choose who may connect")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 560
        standardButtons: Dialog.Ok | Dialog.Cancel
        property var selected: []

        onAboutToShow: {
            groupField.text = page.users.accessGroup !== "" ? page.users.accessGroup : "rdpusers"
            selected = []
        }
        onAccepted: page.users.enableRestriction(groupField.text, selected)

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.m
            Label {
                text: qsTr("Tick the people who may connect remotely. Everyone else will be turned away. You can change this later.")
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
            RowLayout {
                Label {
                    text: qsTr("List name (advanced)")
                    color: Theme.textSecondary
                }
                TextField {
                    id: groupField
                    Layout.fillWidth: true
                    validator: RegularExpressionValidator { regularExpression: /^[a-z_][a-z0-9_-]{0,30}$/ }
                }
            }
            Label {
                text: qsTr("People who may connect")
                font.weight: Font.DemiBold
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.s
                Repeater {
                    model: page.users.model
                    delegate: CheckBox {
                        required property string name
                        text: name
                        onToggled: {
                            const s = enableDialog.selected.filter(n => n !== name)
                            if (checked)
                                s.push(name)
                            enableDialog.selected = s
                        }
                    }
                }
            }
            InlineMessage {
                visible: enableDialog.selected.length === 0
                severity: "warning"
                text: qsTr("Tick at least one person, otherwise nobody could connect.")
            }
        }
    }
}
