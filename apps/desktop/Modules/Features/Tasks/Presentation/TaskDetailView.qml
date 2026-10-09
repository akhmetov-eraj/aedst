import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Media
import Aedst.Feature.Tasks

// One task + its attachments. `taskId` is the route parameter (Router -> StackView
// initial property). Attachments: MediaGrid over TaskDetailViewModel.attachments and a
// MediaUploadPanel bound to TaskDetailViewModel.uploader (uploads carry task_id, which
// attaches them on the server). Activating an attachment opens the media viewer route.
Page {
    id: root

    property string taskId

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: TaskDetailViewModel.title.length > 0 ? TaskDetailViewModel.title : qsTr("Task")

    Component.onCompleted: TaskDetailViewModel.load(root.taskId)

    Shortcut { sequences: [StandardKey.Refresh]; onActivated: TaskDetailViewModel.reload() }

    AedstStateView {
        anchors.fill: parent
        viewState: TaskDetailViewModel.isLoading && !TaskDetailViewModel.hasTask ? AedstStateView.ViewState.Loading
            : !TaskDetailViewModel.hasTask && TaskDetailViewModel.errorMessage.length > 0 ? AedstStateView.ViewState.Error
            : AedstStateView.ViewState.Success
        errorMessage: TaskDetailViewModel.errorMessage
        onRetryRequested: TaskDetailViewModel.reload()

        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: Theme.spacingLg

                Item { Layout.preferredHeight: Theme.spacingSm }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    spacing: Theme.spacingMd

                    AedstCheckBox {
                        checked: TaskDetailViewModel.done
                        enabled: !TaskDetailViewModel.isMutating
                        Accessible.name: TaskDetailViewModel.done ? qsTr("Reopen task") : qsTr("Complete task")
                        onToggled: TaskDetailViewModel.setDone(checked)
                    }
                    AedstLabel {
                        Layout.fillWidth: true
                        text: TaskDetailViewModel.title
                        font: Theme.titleFont
                        color: Theme.foreground
                        wrapMode: Text.Wrap
                    }
                    AedstBadge {
                        visible: TaskDetailViewModel.overdue
                        variant: AedstBadge.Variant.Destructive
                        text: qsTr("Overdue")
                    }
                    AedstButton {
                        text: qsTr("Delete")
                        variant: AedstButton.Variant.Destructive
                        size: AedstButton.Size.Sm
                        enabled: !TaskDetailViewModel.isMutating
                        onClicked: confirmDelete.open()
                    }
                }

                AedstLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    visible: text.length > 0
                    text: TaskDetailViewModel.description
                    font: Theme.bodyFont
                    color: Theme.foreground
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                AedstLabel {
                    Layout.leftMargin: Theme.spacingLg
                    visible: !isNaN(TaskDetailViewModel.dueDate)
                    text: qsTr("Due %1").arg(Qt.formatDateTime(TaskDetailViewModel.dueDate, Locale.ShortFormat))
                    font: Theme.smallFont
                    color: TaskDetailViewModel.overdue ? Theme.destructive : Theme.mutedForeground
                }

                AedstSeparator { Layout.fillWidth: true }

                AedstLabel {
                    Layout.leftMargin: Theme.spacingLg
                    text: qsTr("Attachments (%1)").arg(TaskDetailViewModel.attachments.count)
                    font: Theme.headingFont
                    color: Theme.foreground
                }

                MediaUploadPanel {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    controller: TaskDetailViewModel.uploader
                }

                MediaGrid {
                    id: grid
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    Layout.preferredHeight: Math.max(cellHeight, Math.ceil(count / Math.max(1, Math.floor(width / cellWidth))) * cellHeight)
                    interactive: false
                    deletable: true
                    model: TaskDetailViewModel.attachments
                    onItemActivated: (mediaId, kind) => TaskDetailViewModel.openAttachment(mediaId)
                    onDeleteRequested: (mediaId) => TaskDetailViewModel.deleteAttachment(mediaId)
                }

                AedstLabel {
                    Layout.leftMargin: Theme.spacingLg
                    visible: grid.count === 0
                    text: qsTr("No attachments yet. Drop files above to attach them to this task.")
                    font: Theme.smallFont
                    color: Theme.mutedForeground
                }

                Item { Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }

    AedstAlertDialog {
        id: confirmDelete
        title: qsTr("Delete this task?")
        description: qsTr("The task and its attachments are removed. This cannot be undone.")
        confirmText: qsTr("Delete")
        destructive: true
        onConfirmed: TaskDetailViewModel.deleteTask()
    }
}
