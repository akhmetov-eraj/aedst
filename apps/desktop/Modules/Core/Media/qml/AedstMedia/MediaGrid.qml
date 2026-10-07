import QtQuick
import QtQuick.Controls.Basic
import AedstDesignSystem

// Responsive grid of MediaThumbnail tiles over a MediaListModel. Emits
// itemActivated with the row's id and kind; the owner decides what opens
// (a lightbox for images, VideoPlayer for videos).
GridView {
    id: root

    property int tileSize: 140
    /// Shows a delete button on each tile when true.
    property bool deletable: false

    signal itemActivated(string mediaId, string kind)
    signal deleteRequested(string mediaId)

    clip: true
    cellWidth: tileSize + Theme.spacingSm
    cellHeight: tileSize + Theme.spacingSm
    keyNavigationEnabled: true
    // Two extra rows are created ahead of the viewport so fast scrolling meets tiles whose async image is already loading.
    // reuseItems is deliberately not set: tiles hold image state; measure with AedstMediaScrollBench before enabling it.
    cacheBuffer: cellHeight * 2
    activeFocusOnTab: true
    ScrollBar.vertical: ScrollBar {}

    Accessible.role: Accessible.List
    Accessible.name: qsTr("Media")

    delegate: Item {
        id: cell
        required property string mediaId
        required property string kind
        required property string status
        required property string thumbnailSource
        required property string fileName
        required property string failureReason
        required property int index

        width: root.cellWidth
        height: root.cellHeight

        MediaThumbnail {
            anchors.fill: parent
            anchors.margins: Theme.spacingXs
            kind: cell.kind
            status: cell.status
            thumbnailSource: cell.thumbnailSource
            fileName: cell.fileName
            failureReason: cell.failureReason
            focus: root.currentIndex === cell.index
            onActivated: root.itemActivated(cell.mediaId, cell.kind)
        }

        AedstIconButton {
            visible: root.deletable
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Theme.spacingSm
            iconText: "✕"
            variant: AedstIconButton.Variant.Secondary
            tooltipText: qsTr("Delete %1").arg(cell.fileName)
            Accessible.name: qsTr("Delete %1").arg(cell.fileName)
            onClicked: root.deleteRequested(cell.mediaId)
        }
    }
}
