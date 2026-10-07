import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Feature.Notifications

// Server-side notification preferences (aedst.notifications.v1.Preferences).
// Desktop has no push channel; "Desktop notifications" maps to push_enabled and
// controls the native (tray / notification-center) notifications this app shows
// while it is running. Each switch saves immediately.
Page {
    id: root

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Notification preferences")

    Component.onCompleted: NotificationsViewModel.loadPreferences()

    function save(desktop, completed, dueSoon, mediaReady) {
        NotificationsViewModel.savePreferences(desktop, completed, dueSoon, mediaReady)
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacingLg
        width: Math.min(parent.width - Theme.spacingLg * 2, 520)
        spacing: Theme.spacingMd
        enabled: NotificationsViewModel.preferencesLoaded && !NotificationsViewModel.isSavingPreferences

        Label {
            text: qsTr("Notification preferences")
            font: Theme.titleFont
            color: Theme.foreground
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("While AEDST is running it checks for new notifications in the background and shows them on your desktop. The inbox always lists everything.")
            font: Theme.smallFont
            color: Theme.mutedForeground
            wrapMode: Text.Wrap
        }

        AedstSwitch {
            id: desktop
            text: qsTr("Desktop notifications")
            checked: NotificationsViewModel.desktopNotifications
            onToggled: root.save(checked, completed.checked, dueSoon.checked, media.checked)
        }

        AedstSeparator { Layout.fillWidth: true }

        AedstSwitch {
            id: completed
            enabled: desktop.checked
            text: qsTr("A task was completed")
            checked: NotificationsViewModel.taskCompleted
            onToggled: root.save(desktop.checked, checked, dueSoon.checked, media.checked)
        }
        AedstSwitch {
            id: dueSoon
            enabled: desktop.checked
            text: qsTr("A task is due soon")
            checked: NotificationsViewModel.taskDueSoon
            onToggled: root.save(desktop.checked, completed.checked, checked, media.checked)
        }
        AedstSwitch {
            id: media
            enabled: desktop.checked
            text: qsTr("An upload finished processing")
            checked: NotificationsViewModel.mediaReady
            onToggled: root.save(desktop.checked, completed.checked, dueSoon.checked, checked)
        }

        AedstCircularProgress {
            visible: !NotificationsViewModel.preferencesLoaded || NotificationsViewModel.isSavingPreferences
            running: visible
        }
    }
}
