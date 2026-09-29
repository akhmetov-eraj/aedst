import QtQuick
import QtQuick.Layouts

// Page indicator row -- QML port of shadcn/ui's <Pagination>. Collapses
// distant pages to an ellipsis once `pageCount` exceeds the visible window.
RowLayout {
    id: root

    property int pageCount: 1
    property int currentPage: 0
    property int siblingCount: 1

    signal pageRequested(int page)

    readonly property var pages: {
        const out = []
        for (let i = 0; i < pageCount; ++i) {
            const isEdge = i === 0 || i === pageCount - 1
            const isNear = Math.abs(i - currentPage) <= siblingCount
            if (isEdge || isNear) {
                out.push(i)
            } else if (out[out.length - 1] !== "ellipsis") {
                out.push("ellipsis")
            }
        }
        return out
    }

    spacing: Theme.spacingXs

    Accessible.role: Accessible.PageTabList

    AedstIconButton {
        iconText: "‹"
        variant: AedstIconButton.Variant.Ghost
        implicitWidth: 32
        implicitHeight: 32
        enabled: root.currentPage > 0
        tooltipText: qsTr("Previous page")
        onClicked: root.pageRequested(root.currentPage - 1)
    }

    Repeater {
        model: root.pages

        Item {
            required property var modelData
            implicitWidth: 32
            implicitHeight: 32

            Text {
                anchors.centerIn: parent
                visible: parent.modelData === "ellipsis"
                text: "…"
                color: Theme.mutedForeground
            }

            Rectangle {
                anchors.fill: parent
                visible: parent.modelData !== "ellipsis"
                radius: Theme.radiusMd
                color: parent.modelData === root.currentPage ? Theme.primary : "transparent"
                border.width: parent.modelData === root.currentPage ? 0 : 1
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    text: (parent.parent.modelData + 1).toString()
                    color: parent.parent.modelData === root.currentPage ? Theme.primaryForeground : Theme.foreground
                    font: Theme.smallFont
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.pageRequested(parent.parent.modelData)
                }
            }

            Accessible.role: Accessible.PageTab
            Accessible.name: (modelData === "ellipsis") ? qsTr("More pages") : (modelData + 1).toString()
            Accessible.selected: modelData === root.currentPage
        }
    }

    AedstIconButton {
        iconText: "›"
        variant: AedstIconButton.Variant.Ghost
        implicitWidth: 32
        implicitHeight: 32
        enabled: root.currentPage < root.pageCount - 1
        tooltipText: qsTr("Next page")
        onClicked: root.pageRequested(root.currentPage + 1)
    }
}
