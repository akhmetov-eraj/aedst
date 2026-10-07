import QtQuick
import AedstDesignSystem

// Remote image with a skeleton while loading and a neutral placeholder on error.
// `source` is normally an image://media/... URL from MediaListModel's
// thumbnailSource/previewSource roles (async, cached by MediaImageProvider);
// a file:// URL works too (local previews before upload).
Item {
    id: root

    property url source
    property int fillMode: Image.PreserveAspectCrop
    /// Decode size; defaults to the item size (times the screen's device pixel ratio).
    property size decodeSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio),
                                      Math.ceil(height * Screen.devicePixelRatio))
    property string accessibleName

    readonly property bool loading: image.status === Image.Loading
    readonly property bool failed: image.status === Image.Error
        || (String(source).length === 0 && !loading)

    implicitWidth: 160
    implicitHeight: 120

    Accessible.role: Accessible.Graphic
    Accessible.name: root.accessibleName

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.muted
        visible: root.failed

        Text {
            anchors.centerIn: parent
            text: "\u{1F5BC}"
            font.pixelSize: Math.min(parent.width, parent.height) * 0.3
            color: Theme.mutedForeground
            Accessible.ignored: true
        }
    }

    AedstSkeleton {
        anchors.fill: parent
        visible: root.loading
        Accessible.ignored: true
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        asynchronous: true
        cache: true
        fillMode: root.fillMode
        sourceSize: root.decodeSize
        smooth: true
        mipmap: true
        visible: status === Image.Ready
        Accessible.ignored: true
    }
}
