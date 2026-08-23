// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Window {
    id: root
    width: 360
    height: 158
    visible: notificationServer.hasCurrent
    color: "transparent"
    flags: Qt.FramelessWindowHint
    title: qsTr("Aero7 Notification")

    onVisibleChanged: if (visible && notificationServer.current.timeout > 0) expiry.restart()
    Connections {
        target: notificationServer
        function onCurrentChanged() {
            if (notificationServer.hasCurrent && notificationServer.current.timeout > 0)
                expiry.restart()
            else
                expiry.stop()
        }
    }
    Timer {
        id: expiry
        interval: notificationServer.hasCurrent ? notificationServer.current.timeout : 5000
        running: notificationServer.hasCurrent && notificationServer.current.timeout > 0
        onTriggered: notificationServer.expireCurrent()
    }

    Rectangle {
        anchors.fill: parent
        radius: 5
        gradient: Gradient {
            GradientStop { position: 0; color: "#fdfefe" }
            GradientStop { position: 1; color: "#dcebf4" }
        }
        border.color: "#6797b4"
        border.width: 1
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
            Kirigami.Icon { source: notificationServer.hasCurrent ? notificationServer.current.icon : ""; Layout.preferredWidth: 42; Layout.preferredHeight: 42; Layout.alignment: Qt.AlignTop }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label { Layout.fillWidth: true; text: notificationServer.hasCurrent ? notificationServer.current.summary : ""; font.bold: true; font.pixelSize: 14; color: "#17364c"; elide: Text.ElideRight }
                Label { Layout.fillWidth: true; Layout.fillHeight: true; text: notificationServer.hasCurrent ? notificationServer.current.body : ""; wrapMode: Text.WordWrap; textFormat: Text.PlainText; color: "#324c5e" }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: notificationServer.current.actions || []
                        delegate: Button { required property var modelData; text: modelData.label; onClicked: notificationServer.invokeAction(notificationServer.current.id, modelData.key) }
                    }
                    Item { Layout.fillWidth: true }
                    Label { text: notificationServer.hasCurrent ? notificationServer.current.appName : ""; color: "#667d8d"; font.pixelSize: 10 }
                }
            }
        }
        ToolButton { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 4; icon.name: "window-close"; onClicked: notificationServer.dismissCurrent() }
    }
}
