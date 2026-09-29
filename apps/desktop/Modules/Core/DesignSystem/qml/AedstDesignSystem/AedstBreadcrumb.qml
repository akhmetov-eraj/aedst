import QtQuick
import QtQuick.Layouts

// Path of clickable crumbs -- QML port of shadcn/ui's <Breadcrumb>.
// `model` is an array of {text, value}; the last entry renders as the
// (non-interactive) current page, matching <BreadcrumbPage>.
RowLayout {
    id: root

    property var model: []

    signal activated(var value)

    spacing: Theme.spacingXs

    Accessible.role: Accessible.PathObject

    Repeater {
        model: root.model

        RowLayout {
            id: crumbRow
            required property var modelData
            required property int index
            spacing: Theme.spacingXs
            readonly property bool isLast: index === root.model.length - 1

            Text {
                text: crumbRow.modelData.text
                font: Theme.smallFont
                font.underline: !crumbRow.isLast && crumbArea.containsMouse
                color: crumbRow.isLast ? Theme.foreground : Theme.mutedForeground

                Accessible.role: Accessible.Link
                Accessible.name: crumbRow.modelData.text

                MouseArea {
                    id: crumbArea
                    anchors.fill: parent
                    enabled: !crumbRow.isLast
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(crumbRow.modelData.value)
                }
            }

            Text {
                visible: !crumbRow.isLast
                text: "/"
                color: Theme.mutedForeground
                font: Theme.smallFont
            }
        }
    }
}
