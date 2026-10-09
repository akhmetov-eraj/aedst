import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Feature.Notifications

// In-app inbox. The unread badge lives in the app shell (Main.qml) and binds to
// NotificationsViewModel.unreadCount, which the ViewModel keeps current by polling.
// Keyboard: Enter opens the selected notification (and marks it read),
// Ctrl+Shift+R marks everything read, F5 reloads.
Page {
    id: root

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Notifications")

    Component.onCompleted: NotificationsViewModel.reload()

    Shortcut { sequences: [StandardKey.Refresh]; onActivated: NotificationsViewModel.reload() }
    Shortcut {
        sequence: "Ctrl+Shift+R"
        enabled: NotificationsViewModel.unreadCount > 0
        onActivated: NotificationsViewModel.markAllRead()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            AedstLabel {
                text: qsTr("Notifications")
                font: Theme.titleFont
                color: Theme.foreground
            }
            AedstBadge {
                visible: NotificationsViewModel.unreadCount > 0
                text: qsTr("%n unread", "", NotificationsViewModel.unreadCount)
            }
            Item { Layout.fillWidth: true }
            AedstSwitch {
                text: qsTr("Unread only")
                checked: NotificationsViewModel.unreadOnly
                onToggled: NotificationsViewModel.unreadOnly = checked
            }
            AedstButton {
                text: qsTr("Mark all read")
                variant: AedstButton.Variant.Outline
                size: AedstButton.Size.Sm
                enabled: NotificationsViewModel.unreadCount > 0
                onClicked: NotificationsViewModel.markAllRead()
            }
            AedstButton {
                text: qsTr("Preferences")
                variant: AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                onClicked: NotificationsViewModel.openPreferences()
            }
        }

        AedstStateView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewState: NotificationsViewModel.isLoading && list.count === 0 ? AedstStateView.ViewState.Loading
                : Connectivity.isOffline && list.count === 0 ? AedstStateView.ViewState.Offline
                : NotificationsViewModel.errorMessage.length > 0 && list.count === 0 ? AedstStateView.ViewState.Error
                : list.count === 0 ? AedstStateView.ViewState.Empty
                : AedstStateView.ViewState.Success
            skeleton: true
            errorMessage: NotificationsViewModel.errorMessage
            emptyTitle: qsTr("You're all caught up")
            emptyDescription: qsTr("New notifications appear here and on your desktop.")
            onRetryRequested: NotificationsViewModel.reload()

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: NotificationsViewModel.inbox
                spacing: Theme.spacingXs
                keyNavigationEnabled: true
                activeFocusOnTab: true
                ScrollBar.vertical: ScrollBar {}
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Notification list")
                onAtYEndChanged: if (atYEnd && NotificationsViewModel.hasMore) NotificationsViewModel.loadMore()

                delegate: AedstListItem {
                    id: entry
                    required property int index
                    required property string notificationId
                    required property string title
                    required property string body
                    required property var createdAt
                    required property bool read

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    Accessible.name: entry.title
                    Accessible.description: (entry.read ? "" : qsTr("Unread") + ". ") + entry.body
                    onClicked: NotificationsViewModel.open(entry.index)
                    Keys.onReturnPressed: NotificationsViewModel.open(entry.index)

                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: entry.highlighted || entry.hovered ? Theme.accent : Theme.card
                        border.color: Theme.border
                    }

                    contentItem: RowLayout {
                        spacing: Theme.spacingMd
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 6
                            width: 8; height: 8; radius: 4
                            color: entry.read ? "transparent" : Theme.primary
                            Accessible.ignored: true
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            AedstLabel {
                                Layout.fillWidth: true
                                text: entry.title
                                textFormat: Text.PlainText
                                font: Theme.bodyFont
                                font.weight: entry.read ? Font.Normal : Font.DemiBold
                                color: Theme.foreground
                                elide: Text.ElideRight
                            }
                            AedstLabel {
                                Layout.fillWidth: true
                                visible: entry.body.length > 0
                                text: entry.body
                                textFormat: Text.PlainText
                                font: Theme.smallFont
                                color: Theme.mutedForeground
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                        AedstLabel {
                            Layout.alignment: Qt.AlignTop
                            text: Qt.formatDateTime(entry.createdAt, Locale.ShortFormat)
                            font: Theme.captionFont
                            color: Theme.mutedForeground
                        }
                    }
                }
            }
        }
    }
}
