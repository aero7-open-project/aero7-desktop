// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Window {
    id: root
    required property var targetScreen
    required property string screenName
    width: 320
    height: 48
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint
    title: qsTr("Aero7 Notification Area")

    component TrayButton: Rectangle {
        property alias iconSource: icon.source
        property alias label: labelText.text
        property alias mouseArea: mouse
        signal clicked
        width: labelText.visible ? 48 : 32
        height: 42
        radius: 2
        color: mouse.containsMouse ? "#5a78a8c7" : "transparent"
        Kirigami.Icon { id: icon; anchors.centerIn: parent; width: 22; height: 22 }
        Text { id: labelText; visible: text.length > 0; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; color: "white"; font.pixelSize: 8 }
        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: parent.clicked() }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 3
        spacing: 1

        TrayButton {
            id: hiddenButton
            width: 24
            iconSource: "arrow-up"
            visible: trayController.statusItems.length > 2
            onClicked: hiddenPopup.open()
            ToolTip.visible: mouseArea.containsMouse
            ToolTip.text: qsTr("Show hidden icons")
            Popup {
                id: hiddenPopup
                popupType: Popup.Window
                x: -4
                y: -height - 5
                width: 220
                height: Math.max(70, hiddenGrid.implicitHeight + 22)
                background: Rectangle { color: "#f7fafc"; border.color: "#7999ad"; radius: 4 }
                contentItem: GridLayout {
                    id: hiddenGrid
                    columns: 5
                    Repeater {
                        model: trayController.statusItems.slice(2)
                        delegate: ToolButton {
                            required property var modelData
                            icon.name: modelData.icon
                            display: AbstractButton.IconOnly
                            onClicked: trayController.activateStatusItem(modelData.service, modelData.path, 0, 0)
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.title
                        }
                    }
                }
            }
        }

        Repeater {
            model: trayController.statusItems.slice(0, 2)
            delegate: TrayButton {
                required property var modelData
                iconSource: modelData.icon
                onClicked: trayController.activateStatusItem(modelData.service, modelData.path, 0, 0)
                ToolTip.visible: mouseArea.containsMouse
                ToolTip.text: modelData.title
            }
        }

        TrayButton {
            id: networkButton
            iconSource: trayController.networkingEnabled
                        ? (trayController.networkStatus === "Internet access" ? "network-wired-activated" : "network-offline")
                        : "network-offline"
            onClicked: networkPopup.open()
            ToolTip.visible: mouseArea.containsMouse
            ToolTip.text: trayController.networkStatus
            Popup {
                id: networkPopup
                popupType: Popup.Window
                x: -330
                y: -height - 5
                width: 370
                height: 440
                padding: 0
                background: Rectangle { color: "#f8fafb"; border.color: "#6f92a9"; radius: 5 }
                contentItem: ColumnLayout {
                    spacing: 0
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 58; color: "#dcebf4"
                        Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: trayController.networkStatus; color: "#16364c"; font.pixelSize: 16; font.bold: true }
                        ToolButton { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; icon.name: "view-refresh"; enabled: trayController.networkStatus !== "Network service unavailable"; onClicked: trayController.requestWirelessScan() }
                    }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: trayController.networks
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 62
                            color: hover.hovered ? "#dceef8" : "transparent"
                            Kirigami.Icon { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: 30; height: 30; source: modelData.kind === "wifi" ? (modelData.secured ? "network-wireless-locked" : "network-wireless") : "network-wired" }
                            Column { anchors.left: parent.left; anchors.leftMargin: 52; anchors.right: action.left; anchors.verticalCenter: parent.verticalCenter
                                Text { width: parent.width; text: modelData.name; color: "#172f40"; font.bold: modelData.active; elide: Text.ElideRight }
                                Text { text: modelData.active ? qsTr("Connected") : (modelData.kind === "wifi" ? qsTr("Signal %1%").arg(modelData.signal) : qsTr("Ethernet")); color: "#637785"; font.pixelSize: 11 }
                            }
                            Button { id: action; anchors.right: parent.right; anchors.rightMargin: 9; anchors.verticalCenter: parent.verticalCenter; text: modelData.active ? qsTr("Disconnect") : qsTr("Connect"); onClicked: {
                                    if (modelData.active) trayController.disconnectNetwork(modelData.device)
                                    else if (modelData.secured && modelData.savedConnection.length === 0) { passwordDialog.network = modelData; passwordDialog.open() }
                                    else trayController.connectNetwork(modelData.device, modelData.accessPoint, modelData.name, modelData.savedConnection, "")
                                } }
                            HoverHandler { id: hover }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 54; color: "#edf3f7"
                        CheckBox { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Wi-Fi"); checked: trayController.wirelessEnabled; enabled: trayController.networkStatus !== "Network service unavailable"; onToggled: trayController.setWirelessEnabled(checked) }
                        Button { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Network and Sharing Center"); onClicked: trayController.openControlPanel("network-status") }
                    }
                }
            }
        }

        TrayButton {
            id: volumeButton
            iconSource: !trayController.audioAvailable || trayController.muted ? "audio-volume-muted" : trayController.volume > 55 ? "audio-volume-high" : "audio-volume-medium"
            onClicked: volumePopup.open()
            ToolTip.visible: mouseArea.containsMouse
            ToolTip.text: trayController.audioAvailable ? qsTr("Speakers: %1%").arg(trayController.volume) : qsTr("Audio service unavailable")
            Popup {
                id: volumePopup
                popupType: Popup.Window
                x: -240
                y: -height - 5
                width: 300
                height: 410
                background: Rectangle { color: "#f8fafb"; border.color: "#6f92a9"; radius: 5 }
                contentItem: ColumnLayout {
                    Label { text: trayController.audioAvailable ? qsTr("Speakers") : qsTr("Audio service unavailable"); font.pixelSize: 16; font.bold: true; color: "#17364c" }
                    RowLayout { Layout.fillWidth: true
                        ToolButton { icon.name: trayController.muted ? "audio-volume-muted" : "audio-volume-high"; enabled: trayController.audioAvailable; onClicked: trayController.toggleMute() }
                        Slider { Layout.fillWidth: true; from: 0; to: 150; value: trayController.volume; enabled: trayController.audioAvailable; onMoved: trayController.setVolume(value) }
                        Label { text: trayController.volume + "%" }
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: trayController.outputs
                        textRole: "name"
                        currentIndex: {
                            for (let i = 0; i < trayController.outputs.length; ++i) {
                                if (trayController.outputs[i].default) return i
                            }
                            return -1
                        }
                        onActivated: index => trayController.setDefaultOutput(index)
                    }
                    Label { text: qsTr("Microphone"); font.bold: true; color: "#17364c" }
                    ComboBox {
                        Layout.fillWidth: true
                        model: trayController.inputs
                        textRole: "name"
                        currentIndex: {
                            for (let i = 0; i < trayController.inputs.length; ++i) {
                                if (trayController.inputs[i].default) return i
                            }
                            return -1
                        }
                        enabled: trayController.inputs.length > 0
                        onActivated: index => trayController.setDefaultInput(index)
                    }
                    Label { text: qsTr("Volume Mixer"); font.bold: true; color: "#17364c" }
                    ListView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: trayController.streams
                        delegate: RowLayout { required property int index; required property var modelData; width: ListView.view.width; height: 44
                            Kirigami.Icon { source: modelData.icon; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                            Label { text: modelData.name; Layout.preferredWidth: 80; elide: Text.ElideRight }
                            Slider { Layout.fillWidth: true; from: 0; to: 150; value: modelData.volume; onMoved: trayController.setStreamVolume(index, value) }
                            ToolButton { icon.name: modelData.muted ? "audio-volume-muted" : "audio-volume-high"; onClicked: trayController.setStreamMuted(index, !modelData.muted) }
                        }
                    }
                    Button { Layout.fillWidth: true; text: qsTr("Sound Control Panel"); onClicked: trayController.openControlPanel("sound") }
                }
            }
        }

        TrayButton {
            iconSource: "preferences-system-notifications"
            onClicked: trayController.toggleNotifications(root.screenName)
            ToolTip.visible: mouseArea.containsMouse
            ToolTip.text: qsTr("Notifications")
        }

        TrayButton {
            visible: trayController.removableDevices.length > 0
            iconSource: "device-notifier"
            onClicked: devicesPopup.open()
            ToolTip.visible: mouseArea.containsMouse
            ToolTip.text: qsTr("Removable devices")
            Popup {
                id: devicesPopup
                popupType: Popup.Window
                x: -250
                y: -height - 5
                width: 310
                height: Math.max(100, deviceList.contentHeight + 48)
                background: Rectangle { color: "#f8fafb"; border.color: "#6f92a9"; radius: 5 }
                contentItem: ListView {
                    id: deviceList
                    clip: true
                    model: trayController.removableDevices
                    delegate: RowLayout {
                        required property var modelData
                        width: ListView.view.width
                        height: 58
                        Kirigami.Icon { source: modelData.icon; Layout.preferredWidth: 30; Layout.preferredHeight: 30 }
                        ColumnLayout { Layout.fillWidth: true
                            Label { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight; font.bold: true }
                            Label { text: modelData.mounted ? modelData.path : qsTr("Not mounted"); color: "#637785"; font.pixelSize: 10 }
                        }
                        Button { text: qsTr("Eject"); enabled: modelData.mounted; onClicked: trayController.unmountRemovable(modelData.udi) }
                    }
                }
            }
        }

        TrayButton {
            visible: trayController.batteryPresent
            iconSource: trayController.batteryCharging ? "battery-charging" : "battery-060"
            onClicked: powerPopup.open()
            ToolTip.visible: mouseArea.containsMouse
            ToolTip.text: qsTr("Battery: %1%").arg(Math.round(trayController.batteryPercentage))
            Popup { id: powerPopup; popupType: Popup.Window; x: -180; y: -height - 5; width: 240; height: 120
                background: Rectangle { color: "#f8fafb"; border.color: "#6f92a9"; radius: 5 }
                contentItem: ColumnLayout { Label { text: qsTr("Battery %1%").arg(Math.round(trayController.batteryPercentage)); font.bold: true } Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 14; color: "#d8e1e7"; border.color: "#7891a1"; Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.margins: 2; width: Math.max(0, (parent.width - 4) * trayController.batteryPercentage / 100); color: "#55a84f" } } Button { Layout.fillWidth: true; text: qsTr("Power Options"); onClicked: trayController.openControlPanel("power") } }
            }
        }

        Rectangle {
            width: 74; height: 46; color: clockMouse.containsMouse ? "#4b7495" : "transparent"
            Text { id: clock; anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter; text: Qt.formatDateTime(new Date(), "HH:mm\nM/d/yyyy"); color: "white"; font.pixelSize: 11; style: Text.Raised; styleColor: "#55000000" }
            MouseArea { id: clockMouse; anchors.fill: parent; hoverEnabled: true; onClicked: calendarPopup.open() }
            Timer { interval: 1000; repeat: true; running: true; onTriggered: clock.text = Qt.formatDateTime(new Date(), "HH:mm\nM/d/yyyy") }
            Popup { id: calendarPopup; popupType: Popup.Window; x: -210; y: -height - 5; width: 285; height: 300
                background: Rectangle { color: "#f8fafb"; border.color: "#6f92a9"; radius: 5 }
                contentItem: ColumnLayout { Label { text: Qt.formatDate(new Date(), "dddd, MMMM d, yyyy"); font.pixelSize: 15; font.bold: true; color: "#17364c" } MonthGrid { Layout.fillWidth: true; Layout.fillHeight: true } Button { Layout.fillWidth: true; text: qsTr("Change date and time settings…"); onClicked: trayController.openControlPanel("date-time") } }
            }
        }
    }

    Dialog {
        id: passwordDialog
        property var network
        anchors.centerIn: parent
        title: network ? qsTr("Connect to %1").arg(network.name) : qsTr("Connect")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Column { spacing: 8; Label { text: qsTr("Security key:") } TextField { id: passwordField; echoMode: TextInput.Password; width: 260 } }
        onAccepted: { trayController.connectNetwork(network.device, network.accessPoint, network.name, network.savedConnection, passwordField.text); passwordField.text = "" }
    }
    Dialog { id: errorDialog; anchors.centerIn: parent; property string message; title: qsTr("Aero7 Network"); standardButtons: Dialog.Ok; contentItem: Label { width: 300; wrapMode: Text.WordWrap; text: errorDialog.message } }
    Connections { target: trayController; function onOperationError(title, message) { errorDialog.title = title; errorDialog.message = message; errorDialog.open() } }
}
