// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 470
    height: 410
    visible: true
    title: qsTr("Aero7 Desktop Recovery")
    color: "#edf5fa"
    flags: Qt.Dialog

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 12
        Label { text: qsTr("Aero7 Desktop"); font.pixelSize: 23; font.bold: true; color: "#17364c" }
        Label { Layout.fillWidth: true; text: recoveryController.message; wrapMode: Text.WordWrap; color: "#334f63" }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#a9c4d5" }
        Button { Layout.fillWidth: true; text: qsTr("Restart AeroShell desktop"); onClicked: recoveryController.restartComponent() }
        Button { Layout.fillWidth: true; text: qsTr("Restart complete Aero7 session"); onClicked: recoveryController.restartShell() }
        Button { Layout.fillWidth: true; text: qsTr("Reset corrupted shell state"); onClicked: confirmReset.open() }
        Button { Layout.fillWidth: true; text: qsTr("Open recent logs"); onClicked: recoveryController.openLogs() }
        Item { Layout.fillHeight: true }
        Button { Layout.alignment: Qt.AlignRight; text: qsTr("Sign out"); onClicked: recoveryController.signOut() }
        Label { id: status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#7a3b22" }
    }
    Dialog {
        id: confirmReset
        anchors.centerIn: parent
        width: 390
        title: qsTr("Reset Aero7 shell state?")
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label { width: 330; wrapMode: Text.WordWrap; text: qsTr("Your current shell settings will be moved to a dated recovery backup before known-good defaults are loaded.") }
        onAccepted: recoveryController.resetShellState()
    }
    Connections {
        target: recoveryController
        function onOperationFinished(message, closeWindow) {
            status.text = message
            if (closeWindow) closeTimer.start()
        }
    }
    Timer { id: closeTimer; interval: 800; onTriggered: root.close() }
}
