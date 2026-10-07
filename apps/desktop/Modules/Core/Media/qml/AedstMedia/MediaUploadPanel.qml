import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Media

// Drop zone + "Choose files" button + progress for one MediaUploadController.
// Files come from the native file dialog (QtQuick.Dialogs FileDialog) or are
// dragged onto the panel (DropArea); both feed controller.enqueue(urls).
//
//   MediaUploadPanel { controller: TaskDetailViewModel.uploader }
//
// Keyboard: the "Choose files" button opens the dialog; while uploading,
// Esc cancels; after a failure the Retry button has focus.
FocusScope {
    id: root

    required property MediaUploadController controller
    /// Name filters for the dialog, e.g. ["Images (*.png *.jpg)", "All files (*)"].
    property list<string> nameFilters: [
        qsTr("Images and videos (*.png *.jpg *.jpeg *.webp *.gif *.heic *.mp4 *.mov *.webm *.mkv)"),
        qsTr("All files (*)")
    ]

    readonly property bool active: controller.busy
    readonly property bool isVideo: controller.contentType.startsWith("video/")
    readonly property bool isImage: controller.contentType.startsWith("image/")

    implicitWidth: 360
    implicitHeight: layout.implicitHeight + Theme.spacingMd * 2

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Upload files")

    Keys.onEscapePressed: (event) => {
        if (root.active) {
            root.controller.cancel()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose files to upload")
        fileMode: FileDialog.OpenFiles
        nameFilters: root.nameFilters
        onAccepted: root.controller.enqueue(selectedFiles)
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (event) => {
            if (event.hasUrls) {
                root.controller.enqueue(event.urls)
                event.acceptProposedAction()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLg
        color: drop.containsDrag ? Theme.accent : Theme.card
        border.width: drop.containsDrag ? 2 : 1
        border.color: drop.containsDrag ? Theme.ring : Theme.border
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            // Local preview of the file being uploaded.
            Item {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                visible: root.controller.fileName.length > 0

                MediaImage {
                    anchors.fill: parent
                    visible: root.isImage
                    source: root.isImage ? root.controller.fileUrl : ""
                    accessibleName: root.controller.fileName
                }

                VideoFrameGrabber {
                    id: grabber
                    source: root.isVideo ? root.controller.fileUrl : ""
                    maxPixels: 128
                }

                MediaImage {
                    anchors.fill: parent
                    visible: root.isVideo
                    source: grabber.thumbnailUrl
                    accessibleName: root.controller.fileName
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: root.controller.fileName.length > 0 ? root.controller.fileName
                        : qsTr("Drop files here or choose them")
                    font: Theme.bodyFont
                    color: Theme.cardForeground
                    elide: Text.ElideMiddle
                }

                Text {
                    Layout.fillWidth: true
                    font: Theme.smallFont
                    color: root.controller.state === MediaUploadController.Failed ? Theme.destructive
                                                                                  : Theme.mutedForeground
                    wrapMode: Text.WordWrap
                    text: {
                        switch (root.controller.state) {
                        case MediaUploadController.Preparing: return qsTr("Preparing…")
                        case MediaUploadController.Uploading:
                            return qsTr("Uploading %1%").arg(Math.round(root.controller.progress * 100))
                        case MediaUploadController.Completing: return qsTr("Finishing…")
                        case MediaUploadController.Done: return qsTr("Uploaded")
                        case MediaUploadController.Cancelled: return qsTr("Cancelled")
                        case MediaUploadController.Failed: return root.controller.errorMessage
                        default: return qsTr("Images are resized and compressed before upload.")
                        }
                    }
                }

                Text {
                    visible: root.controller.queuedCount > 0
                    text: qsTr("%n more queued", "", root.controller.queuedCount)
                    font: Theme.captionFont
                    color: Theme.mutedForeground
                }
            }
        }

        AedstProgressBar {
            Layout.fillWidth: true
            visible: root.active
            value: root.controller.progress
            indeterminate: root.controller.state !== MediaUploadController.Uploading
            Accessible.name: qsTr("Upload progress")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            AedstButton {
                text: qsTr("Choose files")
                variant: AedstButton.Variant.Outline
                size: AedstButton.Size.Sm
                enabled: !root.active
                onClicked: fileDialog.open()
            }

            Item { Layout.fillWidth: true }

            AedstButton {
                visible: root.active
                text: qsTr("Cancel")
                variant: AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                onClicked: root.controller.cancel()
            }

            AedstButton {
                id: retryButton
                visible: root.controller.state === MediaUploadController.Failed
                text: qsTr("Retry")
                size: AedstButton.Size.Sm
                onClicked: root.controller.retry()
                onVisibleChanged: if (visible) forceActiveFocus()
            }

            AedstButton {
                visible: root.controller.state === MediaUploadController.Failed
                    || root.controller.state === MediaUploadController.Cancelled
                text: qsTr("Dismiss")
                variant: AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                onClicked: root.controller.reset(false)
            }
        }
    }
}
