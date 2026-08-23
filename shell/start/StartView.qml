// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Window {
    id: root
    width: 520
    height: 590
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint
    title: qsTr("Start")

    onVisibleChanged: {
        if (visible) {
            search.text = ""
            startController.applications.showAll = false
            search.forceActiveFocus()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: "#f7f9fb"
        border.width: 1
        border.color: "#d9f2ff"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 72
            radius: 7
            gradient: Gradient {
                GradientStop { position: 0; color: "#cf4280aa" }
                GradientStop { position: 1; color: "#e31b527a" }
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: startController.userName
                color: "white"
                font.pixelSize: 18
                font.bold: true
                style: Text.Raised
                styleColor: "#60000000"
            }
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                width: 48
                height: 48
                radius: 4
                color: "#eaf3f9"
                border.color: "white"
                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 38
                    height: 38
                    source: startController.userIcon.length > 0 ? startController.userIcon : "user-identity"
                }
            }
        }

        Rectangle {
            id: leftPane
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 72
            anchors.bottom: searchPane.top
            width: 326
            color: "#fbfbfb"

            ListView {
                id: applications
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                spacing: 1
                model: startController.applications
                ScrollBar.vertical: ScrollBar { }

                delegate: Rectangle {
                    id: applicationDelegate
                    required property int index
                    required property string storageId
                    required property string name
                    required property string genericName
                    required property string iconName
                    required property bool pinned
                    required property bool recent
                    width: applications.width
                    height: 46
                    radius: 3
                    color: applicationMouse.containsMouse ? "#d9edf9" : "transparent"
                    border.color: applicationMouse.containsMouse ? "#78a9c6" : "transparent"

                    Kirigami.Icon {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32
                        height: 32
                        source: applicationDelegate.iconName
                    }
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 46
                        anchors.right: pinMark.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Text { width: parent.width; text: applicationDelegate.name; color: "#172332"; elide: Text.ElideRight; font.pixelSize: 13 }
                        Text { width: parent.width; text: applicationDelegate.genericName; color: "#66727d"; elide: Text.ElideRight; font.pixelSize: 10; visible: text.length > 0 }
                    }
                    Text {
                        id: pinMark
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: applicationDelegate.pinned ? "●" : ""
                        color: "#2d6c99"
                    }
                    MouseArea {
                        id: applicationMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        onClicked: mouse => {
                            if (mouse.button === Qt.LeftButton) startController.applications.launch(applicationDelegate.index)
                            else appMenu.popup()
                        }
                    }
                    Menu {
                        id: appMenu
                        MenuItem {
                            text: applicationDelegate.pinned ? qsTr("Unpin from Start Menu") : qsTr("Pin to Start Menu")
                            onTriggered: startController.applications.togglePin(applicationDelegate.index)
                        }
                    }
                }
            }

            Button {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 8
                height: 34
                visible: search.text.length === 0
                text: startController.applications.showAll ? qsTr("Back") : qsTr("All Programs") + "  ▶"
                onClicked: startController.applications.showAll = !startController.applications.showAll
            }
        }

        Rectangle {
            anchors.left: leftPane.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 72
            anchors.bottom: powerPane.top
            color: "#e5f0f7"
            border.color: "#c3dce9"

            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 3
                Repeater {
                    model: [
                        [qsTr("Documents"), "folder-documents", "documents"],
                        [qsTr("Pictures"), "folder-pictures", "pictures"],
                        [qsTr("Music"), "folder-music", "music"],
                        [qsTr("Computer"), "computer", "computer"]
                    ]
                    delegate: Button {
                        required property var modelData
                        width: parent.width
                        height: 38
                        text: modelData[0]
                        icon.name: modelData[1]
                        onClicked: startController.openLocation(modelData[2])
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#abc6d7" }
                Button { width: parent.width; height: 40; text: qsTr("Control Panel"); icon.name: "preferences-system"; onClicked: startController.openControlPanel() }
                Button { width: parent.width; height: 38; text: qsTr("Devices and Printers"); icon.name: "preferences-desktop-peripherals"; onClicked: startController.openDevicesAndPrinters() }
                Button { width: parent.width; height: 38; text: qsTr("Default Programs"); icon.name: "preferences-desktop-filetype-association"; onClicked: startController.openControlPanelSetting("default-apps") }
                Button { width: parent.width; height: 38; text: qsTr("Help and About"); icon.name: "help-about"; onClicked: aboutDialog.open() }
            }
        }

        Rectangle {
            id: searchPane
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: 326
            height: 56
            color: "#eaf1f5"
            TextField {
                id: search
                anchors.fill: parent
                anchors.margins: 10
                placeholderText: qsTr("Search programs and files")
                onTextChanged: startController.applications.query = text
                Keys.onEscapePressed: startController.hide()
            }
        }

        Rectangle {
            id: powerPane
            anchors.left: searchPane.right
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 56
            color: "#d9e8f1"
            Button {
                anchors.left: parent.left
                anchors.right: powerMenuButton.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 10
                text: qsTr("Shut down")
                onClicked: startController.requestPowerAction("shutdown")
            }
            Button {
                id: powerMenuButton
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 36
                text: "▶"
                onClicked: powerMenu.popup()
                Menu {
                    id: powerMenu
                    y: -height
                    MenuItem { text: qsTr("Lock"); onTriggered: startController.lockSession() }
                    MenuItem { text: qsTr("Log off"); onTriggered: startController.requestPowerAction("logout") }
                    MenuSeparator { }
                    MenuItem { text: qsTr("Sleep"); enabled: startController.canSuspend; onTriggered: startController.requestPowerAction("sleep") }
                    MenuItem { text: qsTr("Hibernate"); enabled: startController.canHibernate; onTriggered: startController.requestPowerAction("hibernate") }
                    MenuItem { text: qsTr("Restart"); onTriggered: startController.requestPowerAction("restart") }
                    MenuItem { text: qsTr("Shut down"); onTriggered: startController.requestPowerAction("shutdown") }
                }
            }
        }
    }

    Dialog {
        id: aboutDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("About Aero7 Desktop")
        standardButtons: Dialog.Ok
        width: 390
        contentItem: Column {
            spacing: 8
            Text { text: qsTr("Aero7 Desktop"); font.pixelSize: 22; font.bold: true; color: "#183550" }
            Text { text: qsTr("Version 0.2.0"); color: "#334b60" }
            Text {
                width: 350
                wrapMode: Text.WordWrap
                text: qsTr("Aero7 is an independent Linux desktop built on Wayland, KWin, KDE Frameworks, and open-source Linux infrastructure. It is not Microsoft Windows.")
                color: "#334b60"
            }
            Text { text: qsTr("Computer: ") + startController.hostName; color: "#334b60" }
        }
    }
    Dialog {
        id: errorDialog
        anchors.centerIn: parent
        modal: true
        property string message
        title: qsTr("Aero7 Desktop")
        standardButtons: Dialog.Ok
        contentItem: Text { width: 340; wrapMode: Text.WordWrap; text: errorDialog.message; color: "#334b60" }
    }
    Connections {
        target: startController
        function onOperationError(title, message) {
            errorDialog.title = title
            errorDialog.message = message
            errorDialog.open()
        }
    }
}
