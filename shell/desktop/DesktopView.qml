// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Window {
    id: root
    required property var targetScreen
    required property string screenName
    required property bool primary
    width: targetScreen.geometry.width
    height: targetScreen.geometry.height
    visible: false
    color: "#154c79"
    flags: Qt.FramelessWindowHint
    title: qsTr("Aero7 Desktop")

    Image {
        anchors.fill: parent
        source: "file:" + desktopController.wallpaper
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    GridView {
        id: icons
        visible: root.primary
        anchors.fill: parent
        anchors.margins: 10
        anchors.bottomMargin: 58
        cellWidth: 94
        cellHeight: 92
        flow: GridView.FlowTopToBottom
        model: desktopController.itemsModel
        currentIndex: -1
        highlight: Rectangle { color: "#608ec7e8"; border.color: "#d0f1ff"; radius: 2 }
        delegate: Item {
            id: iconDelegate
            required property int index
            required property string name
            required property string url
            required property string icon
            required property string kind
            width: icons.cellWidth
            height: icons.cellHeight
            Kirigami.Icon { anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 5; width: 48; height: 48; source: icon }
            Text {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 32; text: name; color: "white"; horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                style: Text.Outline; styleColor: "#80000000"; font.pixelSize: 12
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: icons.currentIndex = iconDelegate.index
                onDoubleTapped: desktopController.open(iconDelegate.url)
            }
            TapHandler { acceptedButtons: Qt.RightButton; onTapped: { icons.currentIndex = iconDelegate.index; itemMenu.popup() } }
            Menu {
                id: itemMenu
                MenuItem { text: qsTr("Open"); onTriggered: desktopController.open(iconDelegate.url) }
                MenuItem { text: qsTr("Rename"); enabled: iconDelegate.kind !== "system"; onTriggered: { renameDialog.itemUrl = iconDelegate.url; renameField.text = iconDelegate.name; renameDialog.open() } }
                MenuSeparator { }
                MenuItem { text: qsTr("Move to Recycle Bin"); enabled: iconDelegate.kind !== "system"; onTriggered: desktopController.moveToTrash(iconDelegate.url) }
            }
        }
    }

    Repeater {
        model: desktopController.gadgets
        delegate: Item {
            id: gadgetDelegate
            required property var modelData
            property string gadgetType: String(modelData.type)
            property string gadgetId: String(modelData.id)
            visible: String(modelData.screen).length > 0
                     ? String(modelData.screen) === root.screenName : root.primary
            x: Number(modelData.x)
            y: Number(modelData.y)
            width: gadgetType === "clock" ? 178 : gadgetType === "cpu" ? 205 : 235
            height: gadgetType === "clock" ? 178 : gadgetType === "cpu" ? 115 : 195
            z: 20

            Loader {
                anchors.fill: parent
                sourceComponent: gadgetDelegate.gadgetType === "clock" ? clockGadget
                    : gadgetDelegate.gadgetType === "cpu" ? cpuGadget : notesGadget
            }
            DragHandler {
                id: gadgetDrag
                target: gadgetDelegate
                onActiveChanged: if (!active) {
                    gadgetDelegate.x = Math.max(0, Math.min(root.width - gadgetDelegate.width, gadgetDelegate.x))
                    gadgetDelegate.y = Math.max(0, Math.min(root.height - 58 - gadgetDelegate.height, gadgetDelegate.y))
                    desktopController.setGadgetPosition(gadgetDelegate.gadgetId,
                                                        Math.round(gadgetDelegate.x),
                                                        Math.round(gadgetDelegate.y), root.screenName)
                }
            }
            TapHandler { acceptedButtons: Qt.RightButton; onTapped: gadgetMenu.popup() }
            Menu {
                id: gadgetMenu
                MenuItem { text: qsTr("Remove gadget"); onTriggered: desktopController.removeGadget(gadgetDelegate.gadgetId) }
            }

            Component {
                id: clockGadget
                Rectangle {
                    id: clockFace
                    radius: width / 2
                    border.width: 2
                    border.color: "#d7f3ff"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#d9f5ffdd" }
                        GradientStop { position: 0.45; color: "#6da9cadd" }
                        GradientStop { position: 1.0; color: "#183e5ddd" }
                    }
                    property date now: new Date()
                    Timer {
                        interval: 1000; running: true; repeat: true
                        onTriggered: { clockFace.now = new Date(); hands.requestPaint() }
                    }
                    Repeater {
                        model: 12
                        delegate: Rectangle {
                            required property int index
                            x: clockFace.width / 2 - 1; y: 11
                            width: 2; height: index % 3 === 0 ? 11 : 6; radius: 1; color: "white"
                            transform: Rotation { origin.x: 1; origin.y: clockFace.height / 2 - 11; angle: index * 30 }
                        }
                    }
                    Canvas {
                        id: hands
                        anchors.fill: parent
                        onPaint: {
                            const context = getContext("2d")
                            context.reset()
                            const centerX = width / 2
                            const centerY = height / 2
                            function hand(angle, length, lineWidth, color) {
                                context.save(); context.translate(centerX, centerY); context.rotate(angle)
                                context.beginPath(); context.moveTo(0, 6); context.lineTo(0, -length)
                                context.strokeStyle = color; context.lineWidth = lineWidth; context.lineCap = "round"
                                context.stroke(); context.restore()
                            }
                            hand(((clockFace.now.getHours() % 12) + clockFace.now.getMinutes() / 60) * Math.PI / 6,
                                 height * 0.25, 5, "white")
                            hand(clockFace.now.getMinutes() * Math.PI / 30, height * 0.35, 3, "#f4fbff")
                            hand(clockFace.now.getSeconds() * Math.PI / 30, height * 0.38, 1, "#e54538")
                            context.beginPath(); context.arc(centerX, centerY, 5, 0, Math.PI * 2)
                            context.fillStyle = "white"; context.fill()
                        }
                    }
                }
            }

            Component {
                id: cpuGadget
                Rectangle {
                    radius: 12; border.width: 1; border.color: "#bfe9ff"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#dff7ffee" }
                        GradientStop { position: 0.18; color: "#659fbeee" }
                        GradientStop { position: 1.0; color: "#173853ee" }
                    }
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 7
                        Text { text: qsTr("CPU activity"); color: "white"; font.pixelSize: 14; font.bold: true }
                        Rectangle {
                            width: parent.width; height: 30; radius: 5; color: "#142d3caa"; border.color: "#8ecce8"
                            Rectangle {
                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.margins: 4
                                width: Math.max(4, (parent.width - 8) * desktopController.cpuUsage / 100); radius: 3
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#57e36b" }
                                    GradientStop { position: 0.72; color: "#f0dc45" }
                                    GradientStop { position: 1.0; color: "#f16b4f" }
                                }
                                Behavior on width { NumberAnimation { duration: 450 } }
                            }
                        }
                        Text { text: Math.round(desktopController.cpuUsage) + "%"; color: "#edfaff"; font.pixelSize: 13 }
                    }
                }
            }

            Component {
                id: notesGadget
                Rectangle {
                    radius: 9; border.width: 1; border.color: "#b79d45"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#fffbc9f4" }
                        GradientStop { position: 1.0; color: "#f1d86df4" }
                    }
                    Rectangle {
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        height: 28; radius: 9; color: "#ffffff66"
                        Text { anchors.centerIn: parent; text: qsTr("Aero7 Notes"); color: "#58440b"; font.bold: true }
                    }
                    TextArea {
                        id: noteEditor
                        anchors.fill: parent; anchors.topMargin: 30; anchors.margins: 9
                        text: String(gadgetDelegate.modelData.note); color: "#3f350f"; wrapMode: TextEdit.Wrap
                        background: Item { }
                        onTextChanged: noteSave.restart()
                    }
                    Timer {
                        id: noteSave; interval: 450
                        onTriggered: desktopController.setGadgetNote(gadgetDelegate.gadgetId, noteEditor.text)
                    }
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (drop.hasUrls && desktopController.copyToDesktop(drop.urls.map(String)))
                drop.acceptProposedAction()
        }
    }

    TapHandler { acceptedButtons: Qt.LeftButton; onTapped: icons.currentIndex = -1 }
    TapHandler { acceptedButtons: Qt.RightButton; onTapped: desktopMenu.popup() }
    Menu {
        id: desktopMenu
        MenuItem { text: qsTr("Refresh"); onTriggered: desktopController.refresh() }
        MenuSeparator { }
        MenuItem { text: qsTr("New folder"); onTriggered: { createDialog.folder = true; createField.text = qsTr("New folder"); createDialog.open() } }
        MenuItem { text: qsTr("New text document"); onTriggered: { createDialog.folder = false; createField.text = qsTr("New Text Document.txt"); createDialog.open() } }
        MenuSeparator { }
        MenuItem { text: qsTr("Screen resolution"); onTriggered: desktopController.openControlPanel("display") }
        MenuItem { text: qsTr("Gadgets"); onTriggered: desktopController.openControlPanel("gadgets") }
        MenuItem { text: qsTr("Personalize"); onTriggered: desktopController.openControlPanel("personalization") }
    }

    Dialog {
        id: createDialog
        property bool folder: true
        anchors.centerIn: parent
        title: folder ? qsTr("Create folder") : qsTr("Create file")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: TextField { id: createField; width: 280; selectByMouse: true }
        onAccepted: folder ? desktopController.createFolder(createField.text) : desktopController.createFile(createField.text)
    }
    Dialog {
        id: renameDialog
        property string itemUrl
        anchors.centerIn: parent
        title: qsTr("Rename")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: TextField { id: renameField; width: 280; selectByMouse: true }
        onAccepted: desktopController.renameItem(itemUrl, renameField.text)
    }
    Dialog { id: errorDialog; anchors.centerIn: parent; property string message; title: qsTr("Aero7 Desktop"); standardButtons: Dialog.Ok; contentItem: Label { width: 320; wrapMode: Text.WordWrap; text: errorDialog.message } }
    Connections { target: desktopController; function onOperationError(title, message) { errorDialog.title = title; errorDialog.message = message; errorDialog.open() } }
}
