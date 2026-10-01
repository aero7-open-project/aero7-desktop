import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

Rectangle {
    id: keyboard

    property bool activated: false
    readonly property bool active: activated
    property bool shifted: false
    property var targetField: null
    property var submitCallback: null

    visible: activated
    implicitHeight: 238
    color: "#eef4fa"
    border.color: "#6d8296"
    border.width: 1
    radius: 4

    function insertText(value) {
        if (!targetField) {
            return
        }
        targetField.insert(targetField.cursorPosition, value)
        targetField.forceActiveFocus()
    }

    function backspace() {
        if (!targetField || targetField.cursorPosition <= 0) {
            return
        }
        targetField.remove(targetField.cursorPosition - 1, targetField.cursorPosition)
        targetField.forceActiveFocus()
    }

    function submit() {
        if (submitCallback) {
            submitCallback()
        }
    }

    component KeyButton: QQC2.Button {
        Layout.fillWidth: true
        Layout.fillHeight: true
        focusPolicy: Qt.NoFocus
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]
                KeyButton {
                    text: modelData
                    onClicked: keyboard.insertText(text)
                }
            }
            KeyButton {
                text: "Backspace"
                Layout.preferredWidth: 92
                onClicked: keyboard.backspace()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            Repeater {
                model: ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"]
                KeyButton {
                    text: keyboard.shifted ? modelData.toUpperCase() : modelData
                    onClicked: keyboard.insertText(text)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            spacing: 4
            Repeater {
                model: ["a", "s", "d", "f", "g", "h", "j", "k", "l"]
                KeyButton {
                    text: keyboard.shifted ? modelData.toUpperCase() : modelData
                    onClicked: keyboard.insertText(text)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            KeyButton {
                text: keyboard.shifted ? "Shift on" : "Shift"
                Layout.preferredWidth: 82
                checkable: true
                checked: keyboard.shifted
                onClicked: keyboard.shifted = checked
            }
            Repeater {
                model: ["z", "x", "c", "v", "b", "n", "m"]
                KeyButton {
                    text: keyboard.shifted ? modelData.toUpperCase() : modelData
                    onClicked: keyboard.insertText(text)
                }
            }
            KeyButton {
                text: "Enter"
                Layout.preferredWidth: 82
                onClicked: keyboard.submit()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            KeyButton {
                text: "@"
                onClicked: keyboard.insertText(text)
            }
            KeyButton {
                text: "Space"
                Layout.preferredWidth: 360
                onClicked: keyboard.insertText(" ")
            }
            KeyButton {
                text: "."
                onClicked: keyboard.insertText(text)
            }
            KeyButton {
                text: "-"
                onClicked: keyboard.insertText(text)
            }
        }
    }
}
