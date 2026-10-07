import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Feature.Search

// Search screen. The field writes SearchViewModel.query on every edit; the
// ViewModel debounces (QTimer) and drops stale responses. Highlighted terms are
// rendered from the U+E000/U+E001 markers as escaped StyledText (<b>).
// Keyboard: the field has focus on open; Down moves into suggestions/results,
// Enter searches immediately or opens the selected result.
Page {
    id: root

    background: Rectangle { color: Theme.background }
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Search")

    Component.onCompleted: Qt.callLater(() => searchField.inputItem.forceActiveFocus())
    StackView.onActivated: searchField.inputItem.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        AedstTextField {
            id: searchField
            Layout.fillWidth: true
            label: qsTr("Search")
            placeholderText: qsTr("Search tasks and files")
            accessibleDescription: qsTr("Results update as you type")
            text: SearchViewModel.query
            onEdited: (value) => { SearchViewModel.query = value }
            onAccepted: SearchViewModel.submit()
            Keys.onDownPressed: (suggestionsList.visible ? suggestionsList : resultsList).forceActiveFocus()
        }

        AedstTabBar {
            id: kinds
            Layout.fillWidth: true
            model: [
                { text: qsTr("All"), value: "all" },
                { text: qsTr("Tasks"), value: "task" },
                { text: qsTr("Files"), value: "media" }
            ]
            onCurrentValueChanged: if (currentValue) SearchViewModel.kindFilter = currentValue
        }

        // Suggestions: shown while typing, before results arrive.
        ListView {
            id: suggestionsList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 200)
            visible: count > 0 && searchField.inputFocused && !SearchViewModel.hasSearched
            model: SearchViewModel.suggestions
            keyNavigationEnabled: true
            clip: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Suggestions")
            delegate: ItemDelegate {
                required property string modelData
                width: ListView.view.width
                text: modelData
                onClicked: { SearchViewModel.query = modelData; SearchViewModel.submit() }
                Keys.onReturnPressed: clicked()
            }
        }

        Text {
            visible: SearchViewModel.hasSearched && SearchViewModel.totalEstimate > 0
            text: qsTr("About %n result(s)", "", SearchViewModel.totalEstimate)
            font: Theme.smallFont
            color: Theme.mutedForeground
        }

        AedstStateView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewState: SearchViewModel.isLoading && resultsList.count === 0 ? AedstStateView.ViewState.Loading
                : Connectivity.isOffline ? AedstStateView.ViewState.Offline
                : SearchViewModel.errorMessage.length > 0 ? AedstStateView.ViewState.Error
                : resultsList.count === 0 ? AedstStateView.ViewState.Empty
                : AedstStateView.ViewState.Success
            skeleton: true
            errorMessage: SearchViewModel.errorMessage
            emptyTitle: SearchViewModel.hasSearched ? qsTr("No results") : qsTr("Search your workspace")
            emptyDescription: SearchViewModel.hasSearched ? qsTr("Try different words.")
                                                          : qsTr("Find tasks and files by title or content.")
            onRetryRequested: SearchViewModel.submit()

            ListView {
                id: resultsList
                anchors.fill: parent
                clip: true
                model: SearchViewModel.results
                spacing: Theme.spacingXs
                keyNavigationEnabled: true
                activeFocusOnTab: true
                ScrollBar.vertical: ScrollBar {}
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Search results")
                onAtYEndChanged: if (atYEnd && SearchViewModel.hasMore) SearchViewModel.loadMore()

                delegate: ItemDelegate {
                    id: hit
                    required property int index
                    required property string title
                    required property string snippet
                    required property string snippetPlain
                    required property string kind

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    Accessible.name: hit.title
                    Accessible.description: hit.snippetPlain
                    onClicked: SearchViewModel.openResult(hit.index)
                    Keys.onReturnPressed: SearchViewModel.openResult(hit.index)

                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: hit.highlighted || hit.hovered ? Theme.accent : Theme.card
                        border.color: Theme.border
                    }

                    contentItem: ColumnLayout {
                        spacing: 2
                        RowLayout {
                            AedstBadge {
                                variant: AedstBadge.Variant.Secondary
                                text: hit.kind === "task" ? qsTr("Task") : hit.kind === "media" ? qsTr("File") : hit.kind
                            }
                            Text {
                                Layout.fillWidth: true
                                text: hit.title
                                textFormat: Text.PlainText
                                font: Theme.bodyFont
                                color: Theme.foreground
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: hit.snippet.length > 0
                            // Escaped by SearchResultsModel; only <b> is ever produced.
                            text: hit.snippet
                            textFormat: Text.StyledText
                            font: Theme.smallFont
                            color: Theme.mutedForeground
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
