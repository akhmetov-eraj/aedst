import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Swipeable slide viewer -- QML port of shadcn/ui's <Carousel> (an embla-
// carousel wrapper on web). Built on SwipeView, which already provides the
// same drag-to-advance interaction natively on desktop trackpads/mice.
Item {
    id: root

    default property alias content: view.contentData
    property alias currentIndex: view.currentIndex
    property alias count: view.count
    property bool showArrows: true
    property bool showIndicators: true

    implicitWidth: 480
    implicitHeight: 270

    T.SwipeView {
        id: view
        anchors.fill: parent
        clip: true

        Accessible.role: Accessible.Pane
    }

    AedstIconButton {
        visible: root.showArrows && view.count > 1
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: Theme.spacingSm
        iconText: "‹"
        variant: AedstIconButton.Variant.Secondary
        tooltipText: qsTr("Previous slide")
        enabled: view.currentIndex > 0
        onClicked: view.decrementCurrentIndex()
    }

    AedstIconButton {
        visible: root.showArrows && view.count > 1
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: Theme.spacingSm
        iconText: "›"
        variant: AedstIconButton.Variant.Secondary
        tooltipText: qsTr("Next slide")
        enabled: view.currentIndex < view.count - 1
        onClicked: view.incrementCurrentIndex()
    }

    RowLayout {
        visible: root.showIndicators && view.count > 1
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Theme.spacingSm
        spacing: Theme.spacingXs

        Repeater {
            model: view.count
            Rectangle {
                required property int index
                width: index === view.currentIndex ? 18 : 6
                height: 6
                radius: 3
                color: index === view.currentIndex ? Theme.primary : Theme.mutedForeground
                Behavior on width { NumberAnimation { duration: Theme.durationFast } }
            }
        }
    }
}
