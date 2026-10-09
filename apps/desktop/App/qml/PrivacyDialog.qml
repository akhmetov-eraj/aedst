import QtQuick
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Telemetry

// One-time privacy prompt (both integrations are opt-in and only offered when configured), reopened from the
// toolbar's Privacy button.
AedstDialog {
    id: root

    title: qsTr("Privacy")

    ColumnLayout {
        // The dialog is at most 480 wide and never wider than the window; the preferred width only sets the wrap.
        Layout.fillWidth: true
        Layout.preferredWidth: 440
        spacing: Theme.spacingMd

        AedstLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font: Theme.smallFont
            color: Theme.mutedForeground
            text: qsTr("Help improve AEDST. Crash reports include technical details and your account id only. Usage statistics are anonymous until you sign in and never include what you type.")
        }
        AedstSwitch {
            visible: Telemetry.crashReportsAvailable
            text: qsTr("Send crash reports")
            checked: Telemetry.crashReportsEnabled
            onToggled: Telemetry.crashReportsEnabled = checked
        }
        AedstSwitch {
            visible: Telemetry.analyticsAvailable
            text: qsTr("Share usage statistics")
            checked: Telemetry.analyticsEnabled
            onToggled: Telemetry.analyticsEnabled = checked
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            visible: Telemetry.needsDecision
            AedstButton {
                text: qsTr("No thanks")
                variant: AedstButton.Variant.Ghost
                onClicked: { Telemetry.decideAll(false); root.close() }
            }
            AedstButton {
                text: qsTr("Allow")
                onClicked: { Telemetry.decideAll(true); root.close() }
            }
        }
        AedstButton {
            Layout.alignment: Qt.AlignRight
            visible: !Telemetry.needsDecision
            text: qsTr("Done")
            onClicked: root.close()
        }
    }
}
