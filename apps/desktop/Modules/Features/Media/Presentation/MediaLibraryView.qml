import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Media
import Aedst.Feature.Media

// "My files": upload panel (file dialog + drag & drop), kind filter, media grid.
// Activating a tile opens the viewer route; tiles of items still processing update
// automatically (the ViewModel re-fetches with backoff until variants are ready).
Page {
    id: root

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Files")

    Component.onCompleted: MediaLibraryViewModel.reload()
    Shortcut { sequences: [StandardKey.Refresh]; onActivated: MediaLibraryViewModel.reload() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        AedstLabel {
            text: qsTr("Files")
            font: Theme.titleFont
            color: Theme.foreground
        }

        MediaUploadPanel {
            Layout.fillWidth: true
            controller: MediaLibraryViewModel.uploader
        }

        AedstTabBar {
            Layout.fillWidth: true
            model: [
                { text: qsTr("All"), value: "all" },
                { text: qsTr("Images"), value: "image" },
                { text: qsTr("Videos"), value: "video" },
                { text: qsTr("Documents"), value: "document" }
            ]
            onCurrentValueChanged: if (currentValue) MediaLibraryViewModel.kindFilter = currentValue
        }

        AedstStateView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewState: MediaLibraryViewModel.isLoading && grid.count === 0 ? AedstStateView.ViewState.Loading
                : Connectivity.isOffline && grid.count === 0 ? AedstStateView.ViewState.Offline
                : MediaLibraryViewModel.errorMessage.length > 0 && grid.count === 0 ? AedstStateView.ViewState.Error
                : grid.count === 0 ? AedstStateView.ViewState.Empty
                : AedstStateView.ViewState.Success
            errorMessage: MediaLibraryViewModel.errorMessage
            emptyTitle: qsTr("No files yet")
            emptyDescription: qsTr("Drop images or videos above to upload them.")
            onRetryRequested: MediaLibraryViewModel.reload()

            MediaGrid {
                id: grid
                anchors.fill: parent
                deletable: true
                model: MediaLibraryViewModel.items
                onItemActivated: (mediaId, kind) => MediaLibraryViewModel.open(mediaId)
                onDeleteRequested: (mediaId) => {
                    confirmDelete.mediaId = mediaId
                    confirmDelete.open()
                }
                onAtYEndChanged: if (atYEnd && MediaLibraryViewModel.hasMore) MediaLibraryViewModel.loadMore()
            }
        }
    }

    AedstAlertDialog {
        id: confirmDelete
        property string mediaId
        title: qsTr("Delete this file?")
        description: qsTr("It is removed everywhere it is attached. This cannot be undone.")
        confirmText: qsTr("Delete")
        destructive: true
        onConfirmed: MediaLibraryViewModel.remove(mediaId)
    }
}
