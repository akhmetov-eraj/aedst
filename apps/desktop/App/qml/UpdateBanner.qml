import QtQuick
import QtQuick.Layouts
import AedstDesignSystem

// The self-update banner. `updater` is the aedst::updater::UpdateService (null when updates are off);
// State: 3 Downloading, 4 ReadyToInstall, 5 Available. The owner decides when it is visible and remembers a dismissed
// version.
AedstAlert {
    id: root

    property var updater: null

    signal laterRequested(string version)

    variant: root.updater && root.updater.mandatory ? AedstAlert.Variant.Warning : AedstAlert.Variant.Info
    title: !root.updater ? "" : root.updater.isRollback
        ? qsTr("Returning to version %1").arg(root.updater.availableVersion)
        : qsTr("Version %1 is available").arg(root.updater.availableVersion)
    description: !root.updater ? "" : root.updater.state === 4
        ? qsTr("Restart AEDST to finish updating.")
        : qsTr("Update AEDST with your package manager.")

    RowLayout {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: Theme.spacingMd
        AedstButton {
            visible: root.updater && root.updater.state === 4
            size: AedstButton.Size.Sm
            text: qsTr("Restart to update")
            onClicked: root.updater.installAndRestart()
        }
        AedstButton {
            visible: root.updater && !root.updater.mandatory
            variant: AedstButton.Variant.Ghost
            size: AedstButton.Size.Sm
            text: qsTr("Later")
            onClicked: { root.laterRequested(root.updater.availableVersion); root.updater.dismiss() }
        }
    }
}
