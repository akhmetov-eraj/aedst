import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem

// Blocking screen for a client the server no longer supports (failed_precondition + CLIENT_UPGRADE_REQUIRED,
// ADR-0009). AppDependencies locks the Router on this route, so nothing else can be reached until the app is updated;
// it points at the update flow that already exists (aedst::updater::UpdateService behind the signed manifest, Velopack
// for installed builds).
Page {
    id: root

    // UpdateService of the window (null when updates are off): State 1 Checking, 3 Downloading, 4 ReadyToInstall,
    // 5 Available, 6 Failed. Taken from the ApplicationWindow's `updater` property (Main.qml).
    readonly property var updater: ApplicationWindow.window ? ApplicationWindow.window.updater : null
    readonly property int updaterState: updater ? updater.state : -1
    readonly property bool busy: updaterState === 1 || updaterState === 3

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Update required")

    Component.onCompleted: {
        if (updater && updaterState !== 3 && updaterState !== 4) {
            updater.checkNow()
        }
        checkButton.forceActiveFocus()
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacingLg * 2, 460)
        spacing: Theme.spacingMd

        AedstLabel {
            Layout.fillWidth: true
            text: qsTr("Update required")
            font: Theme.titleFont
            color: Theme.foreground
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        AedstLabel {
            Layout.fillWidth: true
            text: qsTr("This version of AEDST (%1) is no longer supported. Update to continue; changes saved on this device are kept.").arg(Qt.application.version)
            font: Theme.bodyFont
            color: Theme.mutedForeground
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        AedstLabel {
            Layout.fillWidth: true
            visible: text.length > 0
            text: !root.updater ? qsTr("Updates are not available for this installation. Update AEDST with your package manager or install the latest version.")
                : root.busy ? qsTr("Checking for updates...")
                : root.updaterState === 6 ? qsTr("Update failed. Try again later.")
                : ""
            font: Theme.smallFont
            color: Theme.mutedForeground
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Accessible.role: Accessible.StaticText
        }

        AedstCircularProgress {
            Layout.alignment: Qt.AlignHCenter
            visible: root.busy
            running: visible
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingMd

            AedstButton {
                id: checkButton
                visible: root.updater !== null && root.updaterState !== 4
                enabled: !root.busy
                text: qsTr("Check for updates")
                onClicked: root.updater.checkNow()
            }
            AedstButton {
                visible: root.updaterState === 4
                text: qsTr("Restart to update")
                onClicked: root.updater.installAndRestart()
            }
        }
    }
}
