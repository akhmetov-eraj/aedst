import QtQuick
import QtQuick.Controls.Basic as T

// Form field label -- QML port of shadcn/ui's <Label>. A thin themed
// wrapper so every form control shares one label look without repeating
// font/color bindings. `required` appends a themed asterisk without
// mutating the caller's `text`.
T.Label {
    id: root

    property bool required: false

    font: Theme.smallFont
    color: Theme.foreground

    Accessible.role: Accessible.StaticText

    Text {
        visible: root.required
        text: " *"
        color: Theme.destructive
        font: root.font
        x: root.contentWidth
        y: (root.height - height) / 2
    }
}
