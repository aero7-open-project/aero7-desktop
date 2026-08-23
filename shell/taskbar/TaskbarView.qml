// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQml.Models
import org.kde.kirigami as Kirigami
import org.kde.pipewire as PipeWire
import org.kde.taskmanager as TaskManager

Window {
    id: panel
    required property var targetScreen
    required property string screenName

    objectName: "aero7-taskbar-" + screenName
    width: targetScreen.geometry.width
    height: 48
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint
    title: qsTr("Aero7 Taskbar")

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#e0275a82" }
            GradientStop { position: 0.06; color: "#d61b466c" }
            GradientStop { position: 0.52; color: "#dc12385d" }
            GradientStop { position: 1.0; color: "#ed071e39" }
        }
        border.color: "#aa9dcdf0"
        border.width: 1
    }

    ListView {
        id: tasks
        anchors.left: startButton.right
        anchors.right: trayArea.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.rightMargin: 5
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        orientation: ListView.Horizontal
        spacing: 3
        clip: true
        model: taskbarController.tasksModel

        delegate: Item {
            id: task
            required property int index
            required property var model
            property int taskIndex: index
            width: 46
            height: tasks.height

            Drag.active: dragHandler.active
            Drag.source: task
            Drag.hotSpot.x: width / 2
            Drag.hotSpot.y: height / 2
            Drag.mimeData: ({ "application/x-aero7-taskbar-item": String(task.index) })

            Rectangle {
                id: frame
                anchors.fill: parent
                radius: 3
                border.width: 1
                border.color: task.model.IsActive ? "#eaf8ff" : mouse.containsMouse ? "#b8dff7" : "#567c99"
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: task.model.IsActive ? "#b7d8efff"
                              : mouse.containsMouse ? "#9a6aa6ca" : "#443f6685"
                    }
                    GradientStop {
                        position: 0.55
                        color: task.model.IsActive ? "#956aaed9"
                              : mouse.containsMouse ? "#80518caf" : "#332a4e6d"
                    }
                    GradientStop { position: 1; color: "#77203d59" }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    height: 3
                    radius: 1
                    color: task.model.IsDemandingAttention ? "#ff9c21"
                          : task.model.IsActive ? "#68d7ff"
                          : task.model.IsWindow ? "#7ea8c5" : "transparent"

                    SequentialAnimation on opacity {
                        running: task.model.IsDemandingAttention
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.3; to: 1; duration: 450 }
                        NumberAnimation { from: 1; to: 0.3; duration: 450 }
                    }
                }

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    source: task.model.decoration
                    active: mouse.containsMouse
                }

                Rectangle {
                    visible: Number(task.model.ChildCount) > 1
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 3
                    anchors.bottomMargin: 5
                    width: 13
                    height: 13
                    radius: 6
                    color: "#e8edf5"
                    border.color: "#3c5870"
                    Text {
                        anchors.centerIn: parent
                        text: task.model.ChildCount
                        color: "#172939"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                onClicked: mouseEvent => {
                    if (mouseEvent.button === Qt.LeftButton) {
                        taskbarController.activate(task.index)
                    } else if (mouseEvent.button === Qt.MiddleButton) {
                        taskbarController.launchNewInstance(task.index)
                    } else {
                        contextMenu.popup()
                    }
                }
                onContainsMouseChanged: {
                    if (containsMouse) {
                        closePreview.stop()
                        openPreview.restart()
                        const point = task.mapToGlobal(0, 0)
                        taskbarController.publishDelegateGeometry(
                            task.index, Qt.rect(point.x, point.y, task.width, task.height), task)
                    } else {
                        openPreview.stop()
                        closePreview.restart()
                    }
                }
            }

            DragHandler {
                id: dragHandler
                acceptedButtons: Qt.LeftButton
                grabPermissions: PointerHandler.CanTakeOverFromAnything
            }

            DropArea {
                anchors.fill: parent
                keys: ["application/x-aero7-taskbar-item"]
                onEntered: drag => {
                    if (drag.source && drag.source.taskIndex !== task.taskIndex) {
                        taskbarController.move(drag.source.taskIndex, task.taskIndex)
                    }
                }
            }

            Timer {
                id: openPreview
                interval: 450
                onTriggered: {
                    if (task.model.IsWindow || Number(task.model.ChildCount) > 0) {
                        preview.open()
                    }
                }
            }
            Timer {
                id: closePreview
                interval: 180
                onTriggered: {
                    if (!mouse.containsMouse && !previewHover.hovered) {
                        preview.close()
                    }
                }
            }

            ToolTip.visible: mouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: task.model.display || task.model.AppName || qsTr("Application")

            Popup {
                id: preview
                popupType: Popup.Window
                x: Math.round((task.width - width) / 2)
                y: -height - 6
                width: Number(task.model.ChildCount) > 1
                       ? Math.min(680, Number(task.model.ChildCount) * 216 + 14) : 224
                height: 158
                padding: 7
                closePolicy: Popup.CloseOnEscape
                onOpened: taskbarController.beginWindowPeek(task.model.WinIdList || [])
                onClosed: taskbarController.endWindowPeek()

                background: Rectangle {
                    radius: 4
                    color: "#ed173b5a"
                    border.width: 1
                    border.color: "#c4e7fb"
                }

                contentItem: Item {
                    HoverHandler {
                        id: previewHover
                        onHoveredChanged: {
                            if (hovered) {
                                closePreview.stop()
                            } else {
                                closePreview.restart()
                            }
                        }
                    }

                    Rectangle {
                        id: thumbnailFrame
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: caption.top
                        anchors.bottomMargin: 6
                        color: "#111c27"
                        border.color: "#6685a0"
                        clip: true
                        visible: Number(task.model.ChildCount) <= 1

                        TaskManager.ScreencastingRequest {
                            id: screencast
                            uuid: preview.visible && task.model.WinIdList && task.model.WinIdList.length > 0
                                  ? task.model.WinIdList[0] : ""
                        }
                        PipeWire.PipeWireSourceItem {
                            id: liveThumbnail
                            anchors.fill: parent
                            anchors.margins: 1
                            nodeId: screencast.nodeId
                            visible: ready && !task.model.IsMinimized
                        }
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 56
                            height: 56
                            source: task.model.decoration
                            visible: !liveThumbnail.visible
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                taskbarController.activate(task.index)
                                preview.close()
                            }
                        }
                    }

                    Text {
                        id: caption
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 22
                        text: task.model.display || task.model.AppName || qsTr("Application")
                        color: "white"
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                        visible: Number(task.model.ChildCount) <= 1
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 4
                        width: 22
                        height: 20
                        radius: 3
                        color: closeThumbnail.containsMouse ? "#d94b42" : "#7b3540"
                        border.color: "#f4b2ad"
                        visible: Number(task.model.ChildCount) <= 1
                        Text {
                            anchors.centerIn: parent
                            text: "×"
                            color: "white"
                            font.pixelSize: 15
                        }
                        MouseArea {
                            id: closeThumbnail
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: task.model.IsClosable
                            onClicked: {
                                taskbarController.close(task.index)
                                preview.close()
                            }
                        }
                    }


                    DelegateModel {
                        id: groupedWindows
                        model: taskbarController.tasksModel
                        rootIndex: taskbarController.tasksModel.makeModelIndex(task.index)
                        delegate: Item {
                            id: childWindow
                            required property int index
                            required property var model
                            width: 210
                            height: 144

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 2
                                radius: 3
                                color: childHover.hovered ? "#385b75" : "#24455e"
                                border.color: childHover.hovered ? "#bceaff" : "#6e98b4"
                            }
                            Rectangle {
                                id: childFrame
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 7
                                height: 103
                                color: "#111c27"
                                border.color: "#6685a0"
                                clip: true
                                TaskManager.ScreencastingRequest {
                                    id: childCast
                                    uuid: preview.visible && childWindow.model.WinIdList
                                          && childWindow.model.WinIdList.length > 0
                                          ? childWindow.model.WinIdList[0] : ""
                                }
                                PipeWire.PipeWireSourceItem {
                                    id: childThumbnail
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    nodeId: childCast.nodeId
                                    visible: ready && !childWindow.model.IsMinimized
                                }
                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: 48
                                    height: 48
                                    source: childWindow.model.decoration
                                    visible: !childThumbnail.visible
                                }
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.right: childClose.left
                                anchors.leftMargin: 8
                                anchors.rightMargin: 4
                                anchors.bottom: parent.bottom
                                height: 28
                                text: childWindow.model.display || childWindow.model.AppName || qsTr("Window")
                                color: "white"
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 11
                            }
                            Rectangle {
                                id: childClose
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                width: 22
                                height: 20
                                radius: 3
                                color: childCloseMouse.containsMouse ? "#d94b42" : "#7b3540"
                                border.color: "#f4b2ad"
                                Text { anchors.centerIn: parent; text: "×"; color: "white"; font.pixelSize: 15 }
                                MouseArea {
                                    id: childCloseMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: childWindow.model.IsClosable
                                    onClicked: taskbarController.closeChild(task.index, childWindow.index)
                                }
                            }
                            HoverHandler {
                                id: childHover
                                onHoveredChanged: {
                                    if (hovered) taskbarController.beginWindowPeek(childWindow.model.WinIdList || [])
                                    else if (preview.visible) taskbarController.beginWindowPeek(task.model.WinIdList || [])
                                }
                            }
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: {
                                    taskbarController.activateChild(task.index, childWindow.index)
                                    preview.close()
                                }
                            }
                        }
                    }
                    ListView {
                        anchors.fill: parent
                        visible: Number(task.model.ChildCount) > 1
                        orientation: ListView.Horizontal
                        spacing: 4
                        clip: true
                        model: groupedWindows
                    }
                }
            }

            Menu {
                id: contextMenu
                property var jumpItems: []
                onAboutToShow: jumpItems = taskbarController.jumpList(task.index)
                Instantiator {
                    model: contextMenu.jumpItems
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.title
                        icon.name: modelData.icon || ""
                        onTriggered: taskbarController.launchJumpItem(task.index, modelData)
                    }
                    onObjectAdded: (index, object) => contextMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => contextMenu.removeItem(object)
                }
                MenuSeparator { visible: contextMenu.jumpItems.length > 0 }
                MenuItem {
                    text: qsTr("Open new instance")
                    enabled: task.model.CanLaunchNewInstance || task.model.IsLauncher
                    onTriggered: taskbarController.launchNewInstance(task.index)
                }
                MenuItem {
                    text: taskbarController.isPinned(task.model.LauncherUrlWithoutIcon)
                          ? qsTr("Unpin from taskbar") : qsTr("Pin to taskbar")
                    enabled: Boolean(task.model.LauncherUrlWithoutIcon)
                    onTriggered: taskbarController.togglePin(task.index)
                }
                MenuSeparator { }
                MenuItem {
                    text: qsTr("Close window")
                    enabled: task.model.IsClosable
                    onTriggered: taskbarController.close(task.index)
                }
            }
        }
    }

    DropArea {
        anchors.left: tasks.left
        anchors.right: tasks.right
        anchors.top: tasks.top
        anchors.bottom: tasks.bottom
        keys: ["text/uri-list"]
        onDropped: drop => {
            let accepted = false
            for (const url of drop.urls) accepted = taskbarController.pinLauncher(String(url)) || accepted
            if (accepted) drop.acceptProposedAction()
        }
    }

    Rectangle {
        id: startButton
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        width: 50
        height: 42
        radius: 21
        color: startMouse.pressed ? "#2f7c40" : startMouse.containsMouse ? "#55a85c" : "#367f45"
        border.width: 2
        border.color: startMouse.containsMouse ? "#ddfbd8" : "#a9d5b1"
        Text {
            anchors.centerIn: parent
            text: "a7"
            color: "white"
            font.bold: true
            font.italic: true
            font.pixelSize: 17
            style: Text.Raised
            styleColor: "#65000000"
        }
        MouseArea {
            id: startMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: taskbarController.toggleStart(panel.screenName)
        }
        ToolTip.visible: startMouse.containsMouse
        ToolTip.text: qsTr("Start")
    }

    Item {
        id: trayArea
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: showDesktop.left
        width: 320
    }

    Rectangle {
        id: showDesktop
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 12
        color: showMouse.containsMouse ? "#5eacd8" : "#1d4262"
        border.color: "#9dc9e3"
        Accessible.name: qsTr("Show desktop")
        MouseArea {
            id: showMouse
            anchors.fill: parent
            hoverEnabled: true
            onContainsMouseChanged: {
                if (containsMouse) desktopPeekTimer.restart()
                else {
                    desktopPeekTimer.stop()
                    taskbarController.endDesktopPeek()
                }
            }
            onClicked: taskbarController.toggleShowingDesktop()
        }
        Timer {
            id: desktopPeekTimer
            interval: 500
            onTriggered: taskbarController.beginDesktopPeek()
        }
        ToolTip.visible: showMouse.containsMouse
        ToolTip.text: qsTr("Show desktop")
    }

    Dialog {
        id: errorDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Aero7 Taskbar")
        standardButtons: Dialog.Ok
        property string message: ""
        Label { width: 360; wrapMode: Text.Wrap; text: errorDialog.message }
    }

    Connections {
        target: taskbarController
        function onOperationError(title, message) {
            errorDialog.title = title
            errorDialog.message = message
            errorDialog.open()
        }
    }
}
