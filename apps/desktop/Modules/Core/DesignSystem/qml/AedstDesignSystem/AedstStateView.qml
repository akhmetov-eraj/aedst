import QtQuick
import QtQuick.Layouts

// One component for the five states every data-driven screen has:
//   Loading  spinner (or skeleton lines with `skeleton: true`)
//   Empty    title + description + optional action button
//   Error    message + Retry button
//   Offline  AedstOfflineBanner with Retry
//   Success  the default-property content (children of the StateView)
// The property is named `viewState`, not `state`, because Item already owns a
// `state` property (Qt Quick's State machinery).
//
//   AedstStateView {
//       viewState: model.loading ? AedstStateView.ViewState.Loading
//                : Connectivity.isOffline ? AedstStateView.ViewState.Offline
//                : model.failed ? AedstStateView.ViewState.Error
//                : model.count === 0 ? AedstStateView.ViewState.Empty
//                : AedstStateView.ViewState.Success
//       errorMessage: model.errorMessage
//       onRetryRequested: model.reload()
//       ListView { anchors.fill: parent; model: model }
//   }
//
// Accessibility: the root is a pane whose name/description follow the current
// state text; Error and Offline use the alert-message role. Every change of state is also spoken through
// Accessible.announce (assertive for Error and Offline, polite otherwise), so the change is heard without moving
// focus. Content children are hidden, and therefore unfocusable, outside the Success state.
Item {
    id: root

    enum ViewState { Loading, Empty, Error, Offline, Success }

    property int viewState: AedstStateView.ViewState.Success

    property string loadingText: qsTr("Loading…")
    /// Show placeholder lines instead of a spinner while loading.
    property bool skeleton: false

    property string emptyTitle: qsTr("Nothing here yet")
    property string emptyDescription: qsTr("There is nothing to show right now.")
    /// When non-empty an action button is shown in the Empty state.
    property string emptyActionText

    property string errorTitle: qsTr("Something went wrong")
    property string errorMessage: qsTr("Please try again.")

    property string retryText: qsTr("Retry")

    /// Success-state content; children anchor to the StateView's full area.
    default property alias content: contentHost.data

    /// Retry pressed in the Error or Offline state.
    signal retryRequested()
    /// Empty-state action button pressed.
    signal emptyActionTriggered()

    readonly property bool isSuccess: viewState === AedstStateView.ViewState.Success
    readonly property bool isAlert: viewState === AedstStateView.ViewState.Error
        || viewState === AedstStateView.ViewState.Offline

    readonly property string statusTitle: {
        if (viewState === AedstStateView.ViewState.Loading) return loadingText
        if (viewState === AedstStateView.ViewState.Empty) return emptyTitle
        if (viewState === AedstStateView.ViewState.Error) return errorTitle
        if (viewState === AedstStateView.ViewState.Offline) return offlineBanner.title
        return ""
    }
    readonly property string statusDescription: {
        if (viewState === AedstStateView.ViewState.Empty) return emptyDescription
        if (viewState === AedstStateView.ViewState.Error) return errorMessage
        if (viewState === AedstStateView.ViewState.Offline) return offlineBanner.description
        return ""
    }

    implicitWidth: 320
    implicitHeight: 200

    onViewStateChanged: announceState()
    Component.onCompleted: announceState()
    function announceState() {
        if (!root.visible || root.isSuccess || root.statusTitle.length === 0) {
            return
        }
        Accessible.announce(root.statusDescription.length > 0 ? root.statusTitle + ". " + root.statusDescription
                                                              : root.statusTitle,
            root.isAlert ? Accessible.Assertive : Accessible.Polite)
    }

    Accessible.role: root.isAlert ? Accessible.AlertMessage : Accessible.Pane
    Accessible.name: root.statusTitle
    Accessible.description: root.statusDescription

    Item {
        id: contentHost
        anchors.fill: parent
        visible: root.isSuccess
    }

    ColumnLayout {
        id: panel
        anchors.centerIn: parent
        width: Math.min(root.width - Theme.spacingXl * 2, 420)
        visible: !root.isSuccess
        spacing: Theme.spacingMd

        // Loading
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.viewState === AedstStateView.ViewState.Loading
            spacing: Theme.spacingMd

            AedstCircularProgress {
                Layout.alignment: Qt.AlignHCenter
                visible: !root.skeleton
                running: visible
            }

            Text {
                Layout.fillWidth: true
                visible: !root.skeleton
                text: root.loadingText
                font: Theme.smallFont
                color: Theme.mutedForeground
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.skeleton ? 3 : 0

                AedstSkeleton {
                    required property int index
                    Layout.fillWidth: index < 2
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 16
                    Accessible.ignored: true
                }
            }
        }

        // Empty
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.viewState === AedstStateView.ViewState.Empty
            spacing: Theme.spacingSm

            Text {
                Layout.fillWidth: true
                text: root.emptyTitle
                font: Theme.headingFont
                color: Theme.foreground
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.emptyDescription
                font: Theme.smallFont
                color: Theme.mutedForeground
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            AedstButton {
                Layout.alignment: Qt.AlignHCenter
                visible: root.emptyActionText.length > 0
                text: root.emptyActionText
                onClicked: root.emptyActionTriggered()
            }
        }

        // Error
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.viewState === AedstStateView.ViewState.Error
            spacing: Theme.spacingSm

            Text {
                Layout.fillWidth: true
                text: root.errorTitle
                font: Theme.headingFont
                color: Theme.destructive
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.errorMessage
                font: Theme.smallFont
                color: Theme.mutedForeground
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            AedstButton {
                Layout.alignment: Qt.AlignHCenter
                text: root.retryText
                onClicked: root.retryRequested()
            }
        }

        // Offline
        AedstOfflineBanner {
            id: offlineBanner
            Layout.fillWidth: true
            visible: root.viewState === AedstStateView.ViewState.Offline
            retryText: root.retryText
            onRetryRequested: root.retryRequested()
        }
    }
}
