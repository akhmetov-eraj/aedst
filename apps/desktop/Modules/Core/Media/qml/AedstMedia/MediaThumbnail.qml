import QtQuick
import QtQuick.Layouts
import AedstDesignSystem

// One media item as a tile: server thumbnail variant when present; for a video
// without one, a play glyph; documents/audio get a kind glyph. Status overlays
// show processing (spinner) and failure. Bind the MediaListModel roles.
Item {
    id: root

    property string kind          // "image" | "video" | "audio" | "document"
    property string status        // "pending" | "processing" | "ready" | "failed"
    property url thumbnailSource
    property string fileName
    property string failureReason

    signal activated()

    readonly property bool ready: status === "ready"

    implicitWidth: 140
    implicitHeight: 140

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: root.fileName
    Accessible.description: root.status === "failed" ? qsTr("Processing failed: %1").arg(root.failureReason)
        : root.ready ? "" : qsTr("Processing")
    Accessible.onPressAction: root.activated()
    Keys.onReturnPressed: root.activated()
    Keys.onSpacePressed: root.activated()

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.muted
        border.width: root.activeFocus ? 2 : 0
        border.color: Theme.ring
    }

    MediaImage {
        anchors.fill: parent
        anchors.margins: root.activeFocus ? 2 : 0
        visible: String(root.thumbnailSource).length > 0
        source: root.thumbnailSource
        accessibleName: root.fileName
    }

    Text {
        anchors.centerIn: parent
        visible: String(root.thumbnailSource).length === 0
        text: root.kind === "video" ? "▶" : root.kind === "audio" ? "♫" : "\u{1F4C4}"
        font.pixelSize: Math.min(root.width, root.height) * 0.3
        color: Theme.mutedForeground
        Accessible.ignored: true
    }

    // Small play badge on video thumbnails.
    Rectangle {
        visible: root.kind === "video" && String(root.thumbnailSource).length > 0
        anchors.centerIn: parent
        width: 36; height: 36; radius: 18
        color: "#99000000"
        Text { anchors.centerIn: parent; text: "▶"; color: "white"; Accessible.ignored: true }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: "#80000000"
        visible: !root.ready

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacingXs
            AedstCircularProgress {
                Layout.alignment: Qt.AlignHCenter
                visible: root.status !== "failed"
                running: visible
                implicitWidth: 24; implicitHeight: 24
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: root.status === "failed" ? qsTr("Failed") : qsTr("Processing")
                color: "white"
                font: Theme.captionFont
            }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingXs
        text: root.fileName
        elide: Text.ElideMiddle
        color: "white"
        font: Theme.captionFont
        style: Text.Outline
        styleColor: "#80000000"
        Accessible.ignored: true
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: { root.forceActiveFocus(); root.activated() }
    }
}
