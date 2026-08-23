import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ApplicationWindow {
    id: window
    width: 980
    height: 680
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: qsTr("Control Panel")
    color: "#f4f8fc"

    readonly property var categories: [
        "System and Security", "Network and Internet", "Hardware and Sound", "Programs",
        "User Accounts", "Appearance and Personalization", "Clock, Language and Region", "Ease of Access"
    ]

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0; color: "#f9fcff" }
            GradientStop { position: 1; color: "#e5eef7" }
        }
    }

    header: Rectangle {
        height: 86
        color: "#edf5fb"
        border.color: "#a8bfd2"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 7

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ToolButton { text: "‹"; font.pixelSize: 24; enabled: controlPanel.currentPage !== "hub"; Accessible.name: qsTr("Back"); onClicked: controlPanel.currentPage = "hub" }
                ToolButton { text: "›"; font.pixelSize: 24; enabled: false; Accessible.name: qsTr("Forward") }
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    radius: 3
                    color: "white"
                    border.color: "#9bb3c6"
                    Label { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10; text: controlPanel.currentPage === "display" ? qsTr("Control Panel  ›  Display") : controlPanel.currentPage === "defaults" ? qsTr("Control Panel  ›  Default Programs") : controlPanel.currentPage === "personalization" ? qsTr("Control Panel  ›  Personalization") : controlPanel.currentPage === "gadgets" ? qsTr("Control Panel  ›  Desktop Gadgets") : qsTr("Control Panel"); color: "#24445d" }
                }
                TextField {
                    id: search
                    Layout.preferredWidth: 270
                    placeholderText: qsTr("Search Control Panel")
                    text: controlPanel.query
                    onTextChanged: controlPanel.query = text
                    visible: controlPanel.currentPage === "hub"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: controlPanel.currentPage === "display" ? qsTr("Change the appearance of your displays") : controlPanel.currentPage === "defaults" ? qsTr("Choose the programs Aero7 uses by default") : controlPanel.currentPage === "personalization" ? qsTr("Change the visuals and sounds on your computer") : controlPanel.currentPage === "gadgets" ? qsTr("Add useful items to your desktop") : search.text.length ? qsTr("Search results") : qsTr("Adjust your computer's settings"); color: "#234d70"; font.pixelSize: 18 }
                Item { Layout.fillWidth: true }
                Label { text: qsTr("View by:"); visible: controlPanel.currentPage === "hub" }
                ComboBox {
                    model: [qsTr("Category"), qsTr("Large icons"), qsTr("Small icons")]
                    currentIndex: controlPanel.viewMode === "Category" ? 0 : controlPanel.viewMode === "Large icons" ? 1 : 2
                    onActivated: controlPanel.viewMode = currentText
                    visible: controlPanel.currentPage === "hub"
                }
            }
        }
    }

    ScrollView {
        id: hubPage
        anchors.fill: parent
        visible: controlPanel.currentPage === "hub"
        contentWidth: availableWidth
        clip: true

        Flow {
            id: entries
            width: parent.width
            padding: 22
            spacing: 12

            Repeater {
                model: controlPanel.viewMode === "Category" && controlPanel.query.length === 0
                       ? window.categories : controlPanel.settings

                delegate: Rectangle {
                    required property var modelData
                    readonly property bool categoryCard: controlPanel.viewMode === "Category" && controlPanel.query.length === 0
                    width: categoryCard ? Math.max(340, (entries.width - 68) / 2)
                                        : controlPanel.viewMode === "Large icons" ? 210 : Math.max(300, (entries.width - 68) / 2)
                    height: categoryCard ? 128 : controlPanel.viewMode === "Large icons" ? 116 : 58
                    radius: 5
                    color: hover.hovered ? "#d8ecfa" : "#ffffff"
                    border.color: hover.hovered ? "#69a8d3" : "#c8d8e4"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 5

                        RowLayout {
                            Layout.fillWidth: true
                            Kirigami.Icon {
                                Layout.preferredWidth: categoryCard || controlPanel.viewMode === "Large icons" ? 42 : 28
                                Layout.preferredHeight: width
                                source: categoryCard ? "preferences-system" : modelData.icon
                            }
                            Label {
                                Layout.fillWidth: true
                                text: categoryCard ? modelData : modelData.name
                                color: "#17659b"
                                font.pixelSize: categoryCard ? 17 : 15
                                font.bold: true
                                wrapMode: Text.Wrap
                            }
                        }

                        Label {
                            visible: categoryCard
                            Layout.fillWidth: true
                            text: {
                                const names = []
                                for (let index = 0; index < controlPanel.settings.length && names.length < 3; ++index)
                                    if (controlPanel.settings[index].category === modelData) names.push(controlPanel.settings[index].name)
                                return names.join("  ·  ")
                            }
                            color: "#4b6477"
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                        }

                        Label {
                            visible: !categoryCard && controlPanel.viewMode !== "Small icons"
                            Layout.fillWidth: true
                            text: modelData.description || ""
                            color: "#536b7d"
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            font.pixelSize: 12
                        }
                    }

                    HoverHandler { id: hover }
                    TapHandler {
                        onTapped: {
                            if (parent.categoryCard) {
                                search.text = parent.modelData
                            } else {
                                controlPanel.launchSetting(parent.modelData.key)
                            }
                        }
                    }
                }
            }
        }
    }

    ScrollView {
        id: displayPage
        anchors.fill: parent
        visible: controlPanel.currentPage === "display"
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: displayPage.availableWidth
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                Label { text: qsTr("Display configuration"); color: "#174f78"; font.pixelSize: 21; font.bold: true; Layout.fillWidth: true }
                Button { text: qsTr("Detect"); icon.name: "view-refresh"; onClicked: controlPanel.refreshDisplays() }
            }
            Label { Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; text: controlPanel.displayStatus; color: "#536b7d"; wrapMode: Text.Wrap }

            Rectangle {
                id: monitorLayout
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.preferredHeight: 190
                color: "#eaf2f7"
                border.color: "#9db5c6"
                radius: 6
                property real coordinateScale: 0.11
                Label { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 9; text: qsTr("Drag displays to rearrange them"); color: "#36566d"; font.bold: true }
                Repeater {
                    model: controlPanel.displayOutputs
                    delegate: Rectangle {
                        id: monitorBox
                        required property var modelData
                        x: 18 + Math.max(0, Number(modelData.x)) * monitorLayout.coordinateScale
                        y: 43 + Math.max(0, Number(modelData.y)) * monitorLayout.coordinateScale
                        width: 145
                        height: 92
                        radius: 4
                        color: modelData.primary ? "#c8e9fb" : "#ffffff"
                        border.width: modelData.primary ? 2 : 1
                        border.color: modelData.primary ? "#287eae" : "#6f91a8"
                        opacity: modelData.enabled ? 1 : 0.48
                        Label { anchors.centerIn: parent; text: modelData.number; color: "#174f78"; font.pixelSize: 30; font.bold: true }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 5; text: modelData.name; color: "#36566d"; elide: Text.ElideRight; width: parent.width - 10; horizontalAlignment: Text.AlignHCenter }
                        DragHandler {
                            target: monitorBox
                            onActiveChanged: if (!active) {
                                const nextX = Math.max(0, Math.round((monitorBox.x - 18) / monitorLayout.coordinateScale))
                                const nextY = Math.max(0, Math.round((monitorBox.y - 43) / monitorLayout.coordinateScale))
                                const rotations = ({1: "normal", 2: "left", 4: "inverted", 8: "right"})
                                controlPanel.applyDisplay(modelData.id, modelData.currentModeId,
                                                          rotations[modelData.rotation] || "normal",
                                                          modelData.scale, modelData.enabled,
                                                          modelData.primary, nextX, nextY)
                            }
                        }
                    }
                }
            }

            Repeater {
                model: controlPanel.displayOutputs
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.preferredHeight: 270
                    color: "#ffffff"
                    border.color: "#a9bfd0"
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 18

                        Rectangle {
                            Layout.preferredWidth: 190
                            Layout.preferredHeight: 125
                            radius: 5
                            color: "#dcecf7"
                            border.color: "#5c88a6"
                            Label { anchors.centerIn: parent; text: modelData.number; color: "#174f78"; font.pixelSize: 42; font.bold: true }
                            Label { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; text: modelData.name; color: "#36566d" }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            Label { text: qsTr("Resolution:") }
                            ComboBox {
                                id: modeBox
                                Layout.fillWidth: true
                                model: modelData.modes
                                textRole: "name"
                                currentIndex: {
                                    for (let i = 0; i < modelData.modes.length; ++i)
                                        if (modelData.modes[i].id === modelData.currentModeId) return i
                                    return 0
                                }
                            }
                            Label { text: qsTr("Orientation:") }
                            ComboBox {
                                id: rotationBox
                                Layout.fillWidth: true
                                model: [qsTr("Landscape"), qsTr("Portrait (left)"), qsTr("Landscape (flipped)"), qsTr("Portrait (right)")]
                                currentIndex: modelData.rotation === 2 ? 1 : modelData.rotation === 4 ? 2 : modelData.rotation === 8 ? 3 : 0
                                readonly property var values: ["normal", "left", "inverted", "right"]
                            }
                            Label { text: qsTr("Scale:") }
                            SpinBox { id: scaleBox; from: 50; to: 300; stepSize: 25; value: Math.round(modelData.scale * 100); textFromValue: value => value + "%" }
                            Label { text: qsTr("Position:") }
                            RowLayout {
                                Label { text: "X" }
                                SpinBox { id: xBox; from: -10000; to: 10000; value: modelData.x; editable: true }
                                Label { text: "Y" }
                                SpinBox { id: yBox; from: -10000; to: 10000; value: modelData.y; editable: true }
                            }
                            Label { text: qsTr("Options:") }
                            RowLayout {
                                CheckBox { id: enabledBox; text: qsTr("Use this display"); checked: modelData.enabled }
                                CheckBox { id: primaryBox; text: qsTr("Make primary"); checked: modelData.primary; enabled: enabledBox.checked }
                                Label { text: enabledBox.checked ? qsTr("Extend desktop") : qsTr("Disconnected"); color: "#536b7d" }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: qsTr("Apply")
                                    enabled: modeBox.currentIndex >= 0
                                    onClicked: controlPanel.applyDisplay(modelData.id,
                                        modelData.modes[modeBox.currentIndex].id,
                                        rotationBox.values[rotationBox.currentIndex], scaleBox.value / 100,
                                        enabledBox.checked, primaryBox.checked, xBox.value, yBox.value)
                                }
                            }
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.margins: 20
                text: qsTr("Display changes use KScreen and KWin Wayland. Unsafe changes automatically revert after 15 seconds unless confirmed.")
                color: "#536b7d"
                wrapMode: Text.Wrap
            }
        }
    }

    ScrollView {
        id: defaultsPage
        anchors.fill: parent
        visible: controlPanel.currentPage === "defaults"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: defaultsPage.availableWidth
            spacing: 10
            Label { Layout.fillWidth: true; Layout.margins: 20; text: qsTr("Set your default programs"); color: "#174f78"; font.pixelSize: 21; font.bold: true }
            Label { Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; text: qsTr("These choices use the standard Linux MIME and protocol-handler database, so they also apply in File Explorer and other applications."); color: "#536b7d"; wrapMode: Text.Wrap }
            Repeater {
                model: controlPanel.defaultPrograms
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.preferredHeight: 66
                    color: "white"
                    border.color: "#c2d3df"
                    radius: 4
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        Kirigami.Icon { source: modelData.icon; Layout.preferredWidth: 32; Layout.preferredHeight: 32 }
                        Label { text: modelData.name; font.bold: true; color: "#24445d"; Layout.preferredWidth: 110 }
                        ComboBox {
                            id: programBox
                            Layout.fillWidth: true
                            model: modelData.candidates
                            textRole: "name"
                            currentIndex: {
                                for (let i = 0; i < modelData.candidates.length; ++i)
                                    if (modelData.candidates[i].desktopId === modelData.current) return i
                                return 0
                            }
                        }
                        Button {
                            text: qsTr("Set default")
                            enabled: programBox.currentIndex >= 0
                            onClicked: controlPanel.setDefaultProgram(modelData.mimeType,
                                modelData.candidates[programBox.currentIndex].desktopId)
                        }
                    }
                }
            }
        }
    }

    ScrollView {
        id: personalizationPage
        anchors.fill: parent
        visible: controlPanel.currentPage === "personalization"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: personalizationPage.availableWidth
            spacing: 14
            Label { Layout.fillWidth: true; Layout.margins: 20; text: qsTr("Personalization"); color: "#174f78"; font.pixelSize: 21; font.bold: true }
            Rectangle {
                Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; Layout.preferredHeight: 150
                color: "white"; border.color: "#b8cbd9"; radius: 5
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14
                    Label { text: qsTr("Desktop background"); font.pixelSize: 17; font.bold: true; color: "#24445d" }
                    Label { text: qsTr("Choose an original or openly licensed local image. The background is synchronized across Aero7 desktop surfaces."); color: "#536b7d"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { id: wallpaperPath; Layout.fillWidth: true; placeholderText: qsTr("Image file") }
                        Button { text: qsTr("Browse…"); onClicked: wallpaperDialog.open() }
                        Button { text: qsTr("Apply"); enabled: wallpaperPath.text.length > 0; onClicked: controlPanel.setWallpaper(wallpaperPath.text) }
                    }
                }
            }
            Flow {
                Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; spacing: 10
                Button { text: qsTr("Window color"); icon.name: "preferences-desktop-color"; onClicked: controlPanel.launchSetting("colors") }
                Button { text: qsTr("Sounds"); icon.name: "audio-volume-high"; onClicked: controlPanel.launchSetting("sound-theme") }
                Button { text: qsTr("Mouse pointers"); icon.name: "input-mouse"; onClicked: controlPanel.launchSetting("pointers") }
                Button { text: qsTr("Screen saver and lock"); icon.name: "system-lock-screen"; onClicked: controlPanel.launchSetting("screen-lock") }
                Button { text: qsTr("Desktop icons"); icon.name: "user-desktop"; onClicked: controlPanel.launchSetting("folder-options") }
                Button { text: qsTr("Desktop gadgets"); icon.name: "preferences-desktop-widgets"; onClicked: controlPanel.launchSetting("gadgets") }
            }
            Label { Layout.fillWidth: true; Layout.margins: 20; text: qsTr("Aero7 exposes Windows 7-like choices here while keeping generic Plasma theme, containment, and scripting controls hidden."); color: "#536b7d"; wrapMode: Text.Wrap }
        }
    }

    ScrollView {
        id: gadgetsPage
        anchors.fill: parent
        visible: controlPanel.currentPage === "gadgets"
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: gadgetsPage.availableWidth
            spacing: 14
            Label { Layout.fillWidth: true; Layout.margins: 20; text: qsTr("Desktop Gadgets"); color: "#174f78"; font.pixelSize: 21; font.bold: true }
            Label { Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; text: qsTr("Aero7 gadgets use the native desktop surface and remain available even when the Plasma compatibility shell is recovering. Drag a gadget to move it; right-click it to remove it."); color: "#536b7d"; wrapMode: Text.Wrap }
            RowLayout {
                Layout.leftMargin: 20; Layout.rightMargin: 20; spacing: 12
                Button { text: qsTr("Add Clock"); icon.name: "preferences-system-time"; onClicked: controlPanel.addDesktopGadget("clock") }
                Button { text: qsTr("Add CPU Meter"); icon.name: "utilities-system-monitor"; onClicked: controlPanel.addDesktopGadget("cpu") }
                Button { text: qsTr("Add Notes"); icon.name: "document-edit"; onClicked: controlPanel.addDesktopGadget("notes") }
            }
            Label { Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; text: controlPanel.gadgetStatus; visible: text.length > 0; color: "#24445d"; wrapMode: Text.Wrap }
        }
    }

    FileDialog {
        id: wallpaperDialog
        title: qsTr("Choose a desktop background")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.svg *.bmp)"), qsTr("All files (*)")]
        onAccepted: wallpaperPath.text = selectedFile.toString()
    }

    Dialog {
        id: errorDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Control Panel")
        standardButtons: Dialog.Ok
        property string message: ""
        Label { width: 360; wrapMode: Text.Wrap; text: errorDialog.message }
    }

    Dialog {
        id: keepDisplayDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Keep these display settings?")
        standardButtons: Dialog.Yes | Dialog.No
        visible: controlPanel.displayChangePending
        Label { width: 380; wrapMode: Text.Wrap; text: qsTr("Reverting to the previous display settings in %1 seconds.").arg(controlPanel.displayRevertSeconds) }
        onAccepted: controlPanel.confirmDisplayChange()
        onRejected: controlPanel.revertDisplayChange()
    }

    Connections {
        target: controlPanel
        function onOperationError(title, message) {
            errorDialog.title = title
            errorDialog.message = message
            errorDialog.open()
        }
    }
}
