import QtQuick
import QtQuick.Layouts

// Connectivity banner with a Retry action -- shown while the device has no
// network. Purely presentational: bind `visible` to the app's connectivity
// state (e.g. `visible: Connectivity.isOffline`, Aedst.Connectivity) and handle
// `retryRequested` (e.g. `Connectivity.refresh()`). Announced to assistive
// technology as an alert message when it appears.
Rectangle {
    id: root

    property string title: qsTr("You're offline")
    property string description: qsTr("Check your connection. Some features are unavailable until you're back online.")
    property string retryText: qsTr("Retry")
    property bool showRetry: true

    signal retryRequested()

    implicitWidth: 480
    implicitHeight: layout.implicitHeight + Theme.spacingMd * 2
    radius: Theme.radiusLg
    color: Theme.card
    border.width: 1
    border.color: Theme.border

    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.title
    Accessible.description: root.description

    // Spoken when the banner appears (it is shown only while offline).
    function announce() {
        if (root.visible) {
            Accessible.announce(root.title + ". " + root.description, Accessible.Assertive)
        }
    }
    onVisibleChanged: announce()
    Component.onCompleted: announce()

    Rectangle {
        width: 3
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        radius: 1.5
        color: Theme.mutedForeground
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        anchors.leftMargin: Theme.spacingMd + 4
        spacing: Theme.spacingMd

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                font: Theme.bodyFont
                font.weight: Font.DemiBold
                color: Theme.cardForeground
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                font: Theme.smallFont
                color: Theme.mutedForeground
                wrapMode: Text.WordWrap
            }
        }

        AedstButton {
            visible: root.showRetry
            Layout.alignment: Qt.AlignVCenter
            variant: AedstButton.Variant.Outline
            size: AedstButton.Size.Sm
            text: root.retryText
            onClicked: root.retryRequested()
        }
    }
}
