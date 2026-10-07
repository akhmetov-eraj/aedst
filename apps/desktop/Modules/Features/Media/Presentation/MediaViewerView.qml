import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Media
import Aedst.Feature.Media

// Full view of one media item: images through the async, cached image://media
// provider; video/audio through VideoPlayer (Qt Multimedia). `mediaId` is the route
// parameter (from the library, a task attachment, a search hit or a notification).
Page {
    id: root

    property string mediaId

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: MediaViewerViewModel.fileName.length > 0 ? MediaViewerViewModel.fileName : qsTr("File")

    Component.onCompleted: MediaViewerViewModel.load(root.mediaId)
    Component.onDestruction: MediaViewerViewModel.clear()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: MediaViewerViewModel.fileName
                font: Theme.headingFont
                color: Theme.foreground
                elide: Text.ElideMiddle
            }
            Text {
                visible: MediaViewerViewModel.sizeBytes > 0
                text: (MediaViewerViewModel.sizeBytes / (1024 * 1024)).toFixed(1) + " MB"
                font: Theme.captionFont
                color: Theme.mutedForeground
            }
        }

        AedstStateView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewState: MediaViewerViewModel.isLoading && !MediaViewerViewModel.hasMedia ? AedstStateView.ViewState.Loading
                : !MediaViewerViewModel.hasMedia && MediaViewerViewModel.errorMessage.length > 0 ? AedstStateView.ViewState.Error
                : MediaViewerViewModel.status === "failed" ? AedstStateView.ViewState.Error
                : MediaViewerViewModel.status !== "ready" ? AedstStateView.ViewState.Loading
                : AedstStateView.ViewState.Success
            loadingText: MediaViewerViewModel.hasMedia ? qsTr("Processing…") : qsTr("Loading…")
            errorMessage: MediaViewerViewModel.status === "failed" ? MediaViewerViewModel.failureReason
                                                                    : MediaViewerViewModel.errorMessage
            onRetryRequested: MediaViewerViewModel.refresh()

            MediaImage {
                anchors.fill: parent
                visible: MediaViewerViewModel.kind === "image"
                source: visible ? MediaViewerViewModel.previewSource : ""
                fillMode: Image.PreserveAspectFit
                accessibleName: MediaViewerViewModel.fileName
            }

            Loader {
                anchors.fill: parent
                active: MediaViewerViewModel.kind === "video" || MediaViewerViewModel.kind === "audio"
                sourceComponent: VideoPlayer {
                    source: MediaViewerViewModel.playbackUrl
                    posterSource: MediaViewerViewModel.posterSource
                    focus: true
                }
            }

            Text {
                anchors.centerIn: parent
                visible: MediaViewerViewModel.kind === "document"
                text: qsTr("No preview is available for this file type.")
                font: Theme.bodyFont
                color: Theme.mutedForeground
            }
        }
    }
}
