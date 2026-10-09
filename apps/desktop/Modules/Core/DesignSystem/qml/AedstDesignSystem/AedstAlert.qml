import QtQuick
import QtQuick.Layouts

// Inline banner -- QML port of shadcn/ui's <Alert>. `variant` selects the
// semantic tone (info/success/warning/destructive); shadcn's own variant
// prop only has default/destructive, the others are a common extension.
Rectangle {
    id: root

    enum Variant { Info, Success, Warning, Destructive }

    property int variant: AedstAlert.Variant.Info
    property string title
    property string description
    /// Shows a close button and lets Escape close the alert while focus is inside it.
    property bool closable: false

    /// The alert was closed by the user (close button or Escape); the owner decides what that means. By default the
    /// alert hides itself.
    signal closed()

    function close() {
        root.visible = false
        root.closed()
    }

    // Assistive technology hears it when it appears: errors and warnings interrupt, the rest wait their turn.
    function announce() {
        if (root.visible && (title.length > 0 || description.length > 0)) {
            Accessible.announce(title.length > 0 ? title + ". " + description : description,
                variant === AedstAlert.Variant.Destructive || variant === AedstAlert.Variant.Warning
                    ? Accessible.Assertive : Accessible.Polite)
        }
    }
    onVisibleChanged: announce()
    Component.onCompleted: announce()

    Keys.onEscapePressed: (event) => {
        if (root.closable) {
            root.close()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    readonly property color accentColor: {
        if (variant === AedstAlert.Variant.Success) return Theme.success
        if (variant === AedstAlert.Variant.Warning) return Theme.warning
        if (variant === AedstAlert.Variant.Destructive) return Theme.destructive
        return Theme.primary
    }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
    Accessible.description: description

    implicitWidth: 400
    implicitHeight: layout.implicitHeight + Theme.spacingMd * 2
    radius: Theme.radiusLg
    color: Theme.card
    border.width: 1
    border.color: root.accentColor

    Rectangle {
        width: 3
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        radius: 1.5
        color: root.accentColor
    }

    AedstIconButton {
        id: closeButton
        visible: root.closable
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spacingXs
        implicitWidth: 24
        implicitHeight: 24
        iconText: "✕"
        tooltipText: qsTr("Dismiss")
        onClicked: root.close()
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        anchors.leftMargin: Theme.spacingMd + 4
        anchors.rightMargin: root.closable ? Theme.spacingMd + closeButton.width : Theme.spacingMd
        spacing: 2

        Text {
            Layout.fillWidth: true
            visible: root.title.length > 0
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
}
