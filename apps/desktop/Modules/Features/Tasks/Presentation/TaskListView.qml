import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Feature.Tasks

// Signed-in landing screen. Stateless renderer of TaskListViewModel.
// Keyboard: Ctrl+N new task, Ctrl+F filter, F5 reload; Enter/Space on a row opens it.
Page {
    id: root

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Tasks")

    Component.onCompleted: TaskListViewModel.reload()

    Shortcut { sequences: [StandardKey.New]; onActivated: createDialog.open() }
    Shortcut { sequences: [StandardKey.Find]; onActivated: filterField.inputItem.forceActiveFocus() }
    Shortcut { sequences: [StandardKey.Refresh]; enabled: !Connectivity.isOffline; onActivated: TaskListViewModel.reload() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            Label {
                text: TaskListViewModel.totalCount > 0 ? qsTr("Tasks (%1)").arg(TaskListViewModel.totalCount)
                                                       : qsTr("Tasks")
                font: Theme.titleFont
                color: Theme.foreground
            }
            Item { Layout.fillWidth: true }
            AedstSwitch {
                text: qsTr("Show completed")
                checked: TaskListViewModel.showCompleted
                onToggled: TaskListViewModel.showCompleted = checked
            }
            AedstButton {
                text: qsTr("New task")
                onClicked: createDialog.open()
            }
        }

        AedstTextField {
            id: filterField
            Layout.fillWidth: true
            placeholderText: qsTr("Filter tasks")
            accessibleDescription: qsTr("Filters by title and description")
            text: TaskListViewModel.filter
            onEdited: (value) => { TaskListViewModel.filter = value }
        }

        AedstStateView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewState: TaskListViewModel.isLoading && list.count === 0 ? AedstStateView.ViewState.Loading
                : Connectivity.isOffline && list.count === 0 ? AedstStateView.ViewState.Offline
                : TaskListViewModel.errorMessage.length > 0 && list.count === 0 ? AedstStateView.ViewState.Error
                : list.count === 0 ? AedstStateView.ViewState.Empty
                : AedstStateView.ViewState.Success
            skeleton: true
            errorMessage: TaskListViewModel.errorMessage
            emptyTitle: TaskListViewModel.filter.length > 0 ? qsTr("No matching tasks") : qsTr("No tasks yet")
            emptyDescription: qsTr("Create a task to get started.")
            emptyActionText: qsTr("New task")
            onRetryRequested: TaskListViewModel.reload()
            onEmptyActionTriggered: createDialog.open()

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: TaskListViewModel.tasks
                spacing: Theme.spacingXs
                keyNavigationEnabled: true
                activeFocusOnTab: true
                ScrollBar.vertical: ScrollBar {}
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Task list")

                onAtYEndChanged: if (atYEnd && TaskListViewModel.hasMore) TaskListViewModel.loadMore()

                delegate: ItemDelegate {
                    id: row
                    required property string taskId
                    required property string title
                    required property string description
                    required property bool done
                    required property int priority
                    required property var dueDate
                    required property bool overdue
                    required property int index

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    Accessible.name: row.title
                    Accessible.description: (row.done ? qsTr("Completed") : qsTr("Open"))
                        + (row.overdue ? ", " + qsTr("overdue") : "")
                    Keys.onReturnPressed: TaskListViewModel.openTask(row.taskId)
                    onClicked: TaskListViewModel.openTask(row.taskId)

                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: row.highlighted || row.hovered ? Theme.accent : Theme.card
                        border.color: Theme.border
                    }

                    contentItem: RowLayout {
                        spacing: Theme.spacingMd
                        AedstCheckBox {
                            checked: row.done
                            Accessible.name: row.done ? qsTr("Reopen %1").arg(row.title)
                                                      : qsTr("Complete %1").arg(row.title)
                            onToggled: TaskListViewModel.setDone(row.taskId, checked)
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: row.title
                                font: Theme.bodyFont
                                font.strikeout: row.done
                                color: row.done ? Theme.mutedForeground : Theme.foreground
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: row.description.length > 0
                                text: row.description
                                font: Theme.smallFont
                                color: Theme.mutedForeground
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }
                        AedstBadge {
                            visible: row.overdue
                            variant: AedstBadge.Variant.Destructive
                            text: qsTr("Overdue")
                        }
                        Text {
                            visible: row.dueDate !== undefined && !isNaN(row.dueDate)
                            text: visible ? Qt.formatDate(row.dueDate, Locale.ShortFormat) : ""
                            font: Theme.captionFont
                            color: Theme.mutedForeground
                        }
                    }
                }

                footer: Item {
                    width: ListView.view ? ListView.view.width : 0
                    height: TaskListViewModel.hasMore ? 48 : 0
                    AedstCircularProgress {
                        anchors.centerIn: parent
                        visible: TaskListViewModel.hasMore && TaskListViewModel.isLoading
                        running: visible
                    }
                }
            }
        }
    }

    AedstDialog {
        id: createDialog
        title: qsTr("New task")

        property string errorText

        onOpened: { titleField.text = ""; descriptionField.text = ""; errorText = ""; titleField.inputItem.forceActiveFocus() }

        Connections {
            target: TaskListViewModel
            function onTaskCreated(taskId) { createDialog.close() }
            function onCreateFailed(message) { createDialog.errorText = message }
        }

        ColumnLayout {
            width: 420
            spacing: Theme.spacingMd

            AedstTextField {
                id: titleField
                Layout.fillWidth: true
                label: qsTr("Title")
                errorText: createDialog.errorText
                onAccepted: createButton.clicked()
            }
            AedstTextArea {
                id: descriptionField
                Layout.fillWidth: true
                label: qsTr("Description")
            }
            AedstComboBox {
                id: priorityBox
                Layout.fillWidth: true
                model: [qsTr("Low"), qsTr("Medium"), qsTr("High"), qsTr("Urgent")]
                currentIndex: 1
                Accessible.name: qsTr("Priority")
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                AedstButton {
                    text: qsTr("Cancel")
                    variant: AedstButton.Variant.Ghost
                    onClicked: createDialog.close()
                }
                AedstButton {
                    id: createButton
                    text: qsTr("Create")
                    busy: TaskListViewModel.isCreating
                    enabled: titleField.text.trim().length > 0 && !TaskListViewModel.isCreating
                    onClicked: TaskListViewModel.createTask(titleField.text, descriptionField.text,
                                                            priorityBox.currentIndex + 1)
                }
            }
        }
    }
}
