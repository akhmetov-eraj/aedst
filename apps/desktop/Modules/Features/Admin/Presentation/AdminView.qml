import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Feature.Admin

// Operator screen (shown only for the "admin" role; the server enforces it).
// Tabs: Overview counters, Users (debounced search), Audit log, Broadcast form.
Page {
    id: root

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Administration")

    Component.onCompleted: AdminViewModel.reload()
    Shortcut { sequences: [StandardKey.Refresh]; onActivated: AdminViewModel.reload() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        Label {
            text: qsTr("Administration")
            font: Theme.titleFont
            color: Theme.foreground
        }

        AedstAlert {
            Layout.fillWidth: true
            visible: AdminViewModel.forbidden
            variant: AedstAlert.Variant.Destructive
            title: qsTr("Access denied")
            description: qsTr("Your account does not have the admin role (any more). Sign in again if it was just granted.")
        }

        AedstTabBar {
            id: tabs
            Layout.fillWidth: true
            visible: !AdminViewModel.forbidden
            model: [
                { text: qsTr("Overview"), value: "overview" },
                { text: qsTr("Users"), value: "users" },
                { text: qsTr("Audit log"), value: "audit" },
                { text: qsTr("Broadcast"), value: "broadcast" }
            ]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !AdminViewModel.forbidden
            currentIndex: tabs.currentIndex

            // Overview
            GridLayout {
                columns: Math.max(1, Math.floor(width / 220))
                rowSpacing: Theme.spacingMd
                columnSpacing: Theme.spacingMd
                Repeater {
                    model: [
                        { key: "usersTotal", label: qsTr("Users") },
                        { key: "tasksTotal", label: qsTr("Tasks") },
                        { key: "tasksDone", label: qsTr("Tasks done") },
                        { key: "tasksOverdue", label: qsTr("Tasks overdue") },
                        { key: "mediaTotal", label: qsTr("Files") },
                        { key: "mediaBytes", label: qsTr("Storage (MB)") },
                        { key: "pushDevices", label: qsTr("Push devices") },
                        { key: "outboxLagSeconds", label: qsTr("Outbox lag (s)") },
                        { key: "jobsPending", label: qsTr("Jobs pending") }
                    ]
                    AedstCard {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 88
                        Accessible.name: modelData.label
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMd
                            Text {
                                text: modelData.label
                                font: Theme.smallFont
                                color: Theme.mutedForeground
                            }
                            Text {
                                readonly property var raw: AdminViewModel.overview[modelData.key]
                                text: raw === undefined ? "—"
                                    : modelData.key === "mediaBytes" ? (raw / (1024 * 1024)).toFixed(1)
                                    : String(raw)
                                font: Theme.titleFont
                                color: Theme.foreground
                            }
                        }
                    }
                }
            }

            // Users
            ColumnLayout {
                spacing: Theme.spacingSm
                AedstTextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search by email or name")
                    text: AdminViewModel.userQuery
                    onEdited: (value) => { AdminViewModel.userQuery = value }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: AdminViewModel.users
                    keyNavigationEnabled: true
                    activeFocusOnTab: true
                    ScrollBar.vertical: ScrollBar {}
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Users")
                    delegate: ItemDelegate {
                        id: userRow
                        required property string userId
                        required property string email
                        required property string displayName
                        required property var taskCount
                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        Accessible.name: userRow.email
                        contentItem: RowLayout {
                            Text { Layout.fillWidth: true; text: userRow.displayName; textFormat: Text.PlainText; color: Theme.foreground; font: Theme.bodyFont; elide: Text.ElideRight }
                            Text { Layout.fillWidth: true; text: userRow.email; textFormat: Text.PlainText; color: Theme.mutedForeground; font: Theme.smallFont; elide: Text.ElideRight }
                            Text { text: qsTr("%n task(s)", "", Number(userRow.taskCount)); color: Theme.mutedForeground; font: Theme.captionFont }
                            AedstButton {
                                text: qsTr("Notify")
                                size: AedstButton.Size.Sm
                                variant: AedstButton.Variant.Ghost
                                onClicked: { broadcastUser.text = userRow.userId; tabs.currentIndex = 3 }
                            }
                        }
                    }
                }
            }

            // Audit log
            ListView {
                clip: true
                model: AdminViewModel.auditLog
                keyNavigationEnabled: true
                activeFocusOnTab: true
                ScrollBar.vertical: ScrollBar {}
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Audit log")
                delegate: ItemDelegate {
                    id: auditRow
                    required property string action
                    required property string targetKind
                    required property string targetId
                    required property string reason
                    required property string actorId
                    required property var createdAt
                    width: ListView.view.width
                    Accessible.name: auditRow.action
                    contentItem: ColumnLayout {
                        spacing: 2
                        Text {
                            text: auditRow.action + " · " + auditRow.targetKind + " " + auditRow.targetId
                            textFormat: Text.PlainText
                            font: Theme.bodyFont
                            color: Theme.foreground
                        }
                        Text {
                            text: Qt.formatDateTime(auditRow.createdAt, Locale.ShortFormat) + " · " + auditRow.actorId
                                + (auditRow.reason.length > 0 ? " · " + auditRow.reason : "")
                            textFormat: Text.PlainText
                            font: Theme.captionFont
                            color: Theme.mutedForeground
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            // Broadcast
            ColumnLayout {
                spacing: Theme.spacingMd

                Connections {
                    target: AdminViewModel
                    function onBroadcastSent(recipients) { broadcastStatus.text = qsTr("Sent to %n user(s).", "", recipients) }
                    function onBroadcastFailed(message) { broadcastStatus.text = message }
                }

                AedstTextField { id: broadcastTitle; Layout.fillWidth: true; label: qsTr("Title") }
                AedstTextArea { id: broadcastBody; Layout.fillWidth: true; label: qsTr("Message") }
                AedstTextField {
                    id: broadcastLink
                    Layout.fillWidth: true
                    label: qsTr("Link (optional)")
                    placeholderText: "/tasks"
                }
                AedstTextField {
                    id: broadcastUser
                    Layout.fillWidth: true
                    label: qsTr("User id (empty = everyone)")
                }
                RowLayout {
                    AedstButton {
                        text: broadcastUser.text.length > 0 ? qsTr("Send to user") : qsTr("Send to everyone")
                        variant: broadcastUser.text.length > 0 ? AedstButton.Variant.Default : AedstButton.Variant.Destructive
                        busy: AdminViewModel.isBroadcasting
                        enabled: broadcastTitle.text.trim().length > 0 && !AdminViewModel.isBroadcasting
                        onClicked: AdminViewModel.broadcast(broadcastTitle.text, broadcastBody.text,
                                                            broadcastLink.text, broadcastUser.text)
                    }
                    Text { id: broadcastStatus; font: Theme.smallFont; color: Theme.mutedForeground }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
