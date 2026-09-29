import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Multi-box one-time-password input -- QML port of shadcn/ui's
// <InputOTP>. Exposes the joined digits as `value` and emits `completed`
// once every box is filled.
RowLayout {
    id: root

    property int length: 6
    property string value: ""
    property var boxes: []

    signal completed(string code)

    spacing: Theme.spacingSm

    function clear() {
        for (const box of boxes) box.text = ""
        if (boxes.length > 0) boxes[0].forceActiveFocus()
    }

    Repeater {
        model: root.length
        delegate: T.TextField {
            id: box
            required property int index
            Layout.preferredWidth: 40
            Layout.preferredHeight: 48
            horizontalAlignment: Text.AlignHCenter
            font: Theme.headingFont
            color: Theme.foreground
            maximumLength: 1
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator { regularExpression: /[0-9]/ }

            Component.onCompleted: {
                const next = root.boxes.slice()
                next[index] = box
                root.boxes = next
            }

            Accessible.role: Accessible.EditableText
            Accessible.name: qsTr("Digit %1 of %2").arg(index + 1).arg(root.length)

            onTextChanged: {
                if (text.length === 1 && index < root.length - 1) {
                    root.boxes[index + 1].forceActiveFocus()
                }
                root.value = root.boxes.map(b => b.text).join("")
                if (root.value.length === root.length) {
                    root.completed(root.value)
                }
            }

            Keys.onBackPressed: {
                if (text.length === 0 && index > 0) {
                    root.boxes[index - 1].forceActiveFocus()
                }
            }

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.background
                border.width: box.activeFocus ? 2 : 1
                border.color: box.activeFocus ? Theme.ring : Theme.input
            }
        }
    }
}
