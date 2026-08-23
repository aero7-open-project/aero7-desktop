// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Window {
    id: root
    width: 390
    height: 520
    visible: notificationServer.historyVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint
    title: qsTr("Aero7 Notification History")

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: "#f7fafc"
        border.color: "#6f92a9"
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 58; color: "#dcebf4"
                Label { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Notifications"); font.pixelSize: 18; font.bold: true; color: "#17364c" }
                ToolButton { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; icon.name: "window-close"; onClicked: notificationServer.historyVisible = false }
            }
            Label { Layout.fillWidth: true; Layout.fillHeight: true; visible: notificationServer.history.length === 0; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: qsTr("No notifications"); color: "#657b89" }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; visible: notificationServer.history.length > 0; clip: true; model: notificationServer.history
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: Math.max(82, content.implicitHeight + 18); color: hover.hovered ? "#e3f0f7" : "transparent"
                    RowLayout { id: content; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 9
                        Kirigami.Icon { source: modelData.icon; Layout.preferredWidth: 34; Layout.preferredHeight: 34; Layout.alignment: Qt.AlignTop }
                        ColumnLayout { Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: modelData.summary; font.bold: true; color: "#17364c"; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; text: modelData.body; wrapMode: Text.WordWrap; textFormat: Text.PlainText; color: "#405867"; maximumLineCount: 3; elide: Text.ElideRight }
                            Label { text: modelData.appName + "  " + Qt.formatDateTime(new Date(modelData.timestamp), "HH:mm"); color: "#718692"; font.pixelSize: 10 }
                        }
                    }
                    HoverHandler { id: hover }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 48; color: "#edf3f7"
                Button { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Clear all"); enabled: notificationServer.history.length > 0; onClicked: notificationServer.clearHistory() }
            }
        }
    }
}
