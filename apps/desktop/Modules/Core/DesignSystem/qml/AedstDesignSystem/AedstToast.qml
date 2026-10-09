import QtQuick
import QtQuick.Layouts

// Auto-dismissing overlay notification -- QML port of shadcn/ui's
// <Toast>/sonner usage. Not a manager/queue (there is no global overlay
// singleton in this module yet); a view instantiates one per active
// notification and anchors it, e.g. to a window corner.
Rectangle {
    id: root

    enum Variant { Default, Success, Warning, Destructive }

    property int variant: AedstToast.Variant.Default
    property string title
    property string description
    property int duration: 4000
    property bool dismissible: true

    signal dismissed()

    readonly property color accentColor: {
        if (variant === AedstToast.Variant.Success) return Theme.success
        if (variant === AedstToast.Variant.Warning) return Theme.warning
        if (variant === AedstToast.Variant.Destructive) return Theme.destructive
        return Theme.primary
    }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
    Accessible.description: description

    implicitWidth: 340
    implicitHeight: layout.implicitHeight + Theme.spacingMd * 2
    radius: Theme.radiusLg
    color: Theme.card
    border.width: 1
    border.color: Theme.border

    layer.enabled: true

    opacity: 0
    Component.onCompleted: {
        showAnimation.start()
        // A toast is a transient status message: announced politely (errors interrupt), without moving focus.
        Accessible.announce(description.length > 0 ? title + ". " + description : title,
            variant === AedstToast.Variant.Destructive ? Accessible.Assertive : Accessible.Polite)
    }

    Keys.onEscapePressed: (event) => {
        if (root.dismissible) {
            root.close()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    NumberAnimation { id: showAnimation; target: root; property: "opacity"; to: 1; duration: Theme.durationNormal }

    Timer {
        interval: root.duration
        running: root.duration > 0
        onTriggered: root.close()
    }

    function close() {
        hideAnimation.start()
    }
    SequentialAnimation {
        id: hideAnimation
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: Theme.durationFast }
        ScriptAction { script: root.dismissed() }
    }

    Rectangle {
        width: 3
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        radius: 1.5
        color: root.accentColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        anchors.leftMargin: Theme.spacingMd + 4
        spacing: Theme.spacingSm

        ColumnLayout {
            id: layout
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

        AedstIconButton {
            visible: root.dismissible
            iconText: "✕"
            variant: AedstIconButton.Variant.Ghost
            implicitWidth: 24
            implicitHeight: 24
            tooltipText: qsTr("Dismiss")
            onClicked: root.close()
        }
    }
}
