import QtQuick
import SddmComponents
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtQuick.Controls as QQC2
import "SMOD" as SMOD
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasma5support as Plasma5Support
import org.kde.kitemmodels as KItemModels

import QtMultimedia

Item
{
    id: root

    LayoutMirroring.enabled : Qt.locale().textDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit : true

    property bool m_forceUserSelect: config.boolValue("forceUserSelect")
    property bool m_biggerUserFrame: config.boolValue("biggerUserFrame")
    property bool m_biggerMultiUserFrame: config.boolValue("biggerMultiUserFrame")
    property bool highContrastEnabled: false
    property bool magnifierEnabled: false

    enum LoginPage
    {
        Startup,
        SelectUser,
        Login,
        LoginFailed
    }

    // copied from sddm/src/greeter/UserModel.h because
    // it doesn't seem accessible via e.g. SDDM.UserModel.NameRole
    enum UserRoles
    {
        NameRole = 257,
        RealNameRole,
        HomeDirRole,
        IconRole,
        NeedsPasswordRole
    }

    Connections
    {
        target: sddm

        function onLoginFailed()
        {
            password.text = ""
            pages.currentIndex = Main.LoginPage.LoginFailed
        }
    }

    Keys.onEscapePressed:
    {
        if (pages.currentIndex === Main.LoginPage.Login && switchuser.enabled)
        {
            password.text = ""
            pages.currentIndex = Main.LoginPage.SelectUser
        }
        else if (pages.currentIndex === Main.LoginPage.LoginFailed)
        {
            pages.currentIndex = Main.LoginPage.Login
        }
    }

    Rectangle {
        color: root.highContrastEnabled ? "#000000" : "#1D5F7A"
        anchors.fill: parent
    }

    Background {
        id: background
        anchors.fill: parent
        fillMode: Image.Stretch
        source: Qt.resolvedUrl("background")
        visible: !root.highContrastEnabled
    }

    Timer {
        id: startupSoundDelay
        interval: config.boolValue("enableStartup") ? 250 : 20
        running: startupSound.playSound
        onTriggered: {
            startupSound.play()
        }
    }

    MediaPlayer {
        id: startupSound
        property bool playSound: !executable.fileExists && config.boolValue("playSound")
        audioOutput: AudioOutput {
            volume: 1.0
        }
        source: "Assets/session-start.wav"
    }
    Component
    {
        id: userDelegate

        // Children use explicit anchors; a positioner must not own their y positions.
        Item
        {
            id: delegateColumn
            width: listView.cellWidth
            height: listView.cellHeight

            Item
            {
                id: avatarparent
                width: m_biggerMultiUserFrame ? 100 : 80
                height: width

                anchors.centerIn: parent

                Item
                {
                    width: m_biggerMultiUserFrame ? 60 : 48
                    height: width

                    anchors.centerIn: parent

                    Rectangle
                    {
                        id: maskmini

                        anchors.fill: parent
                        anchors.centerIn: parent
                        radius: 2
                        visible: false
                    }

                    LinearGradient {
                        id: gradient
                        anchors.fill: parent
                        anchors.centerIn: parent
                        z: -1
                        start: Qt.point(0,0)
                        end: Qt.point(gradient.width, gradient.height)
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#eeecee" }
                            GradientStop { position: 1.0; color: "#a39ea3" }
                        }
                    }
                    Image
                    {
                        id: avatarmini

                        property string m_fallbackPicture: "Assets/user.png"

                        onStatusChanged:
                        {
                            if (avatarmini.status == Image.Error)
                            {
                                avatarmini.source = avatarmini.m_fallbackPicture;
                            }
                        }

                        source: model.icon

                        fillMode: Image.PreserveAspectCrop

                        anchors.fill: parent
                        anchors.centerIn: parent

                        layer.enabled: true
                        layer.effect: OpacityMask
                        {
                            maskSource: maskmini
                        }
                    }
                }

                Image
                {
                    id: avatarminiframe

                    property bool m_hovered: false

                    source:
                    {
                        if (m_biggerMultiUserFrame)
                        {
                            if (m_hovered && delegateColumn.focus)   return "Assets/12235.png"
                            if (m_hovered && !delegateColumn.focus)  return "Assets/12233.png"
                            if (!m_hovered && delegateColumn.focus)  return "Assets/12234.png"
                            if (!m_hovered && !delegateColumn.focus) return "Assets/12237.png"
                        }
                        else
                        {
                            if (m_hovered && delegateColumn.focus)   return "Assets/12220.png"
                            if (m_hovered && !delegateColumn.focus)  return "Assets/12218.png"
                            if (!m_hovered && delegateColumn.focus)  return "Assets/12219.png"
                            if (!m_hovered && !delegateColumn.focus) return "Assets/12222.png"
                        }
                    }
                    anchors.fill: parent
                    anchors.centerIn: parent
                }
            }

            Text
            {
                text: (model.realName === "") ? model.name : model.realName
                color: "white"
                font.pixelSize: 12
                //font.family: mainfont.name

                renderType: Text.NativeRendering
                font.hintingPreference: Font.PreferFullHinting
                font.kerning: false
                anchors.top: avatarparent.bottom
                //anchors.topMargin: -3
                anchors.horizontalCenter: avatarparent.horizontalCenter

                layer.enabled: true
                layer.effect: DropShadow
                {
                    verticalOffset: 1
                    color: "#000"
                    radius: 7
                    samples: 20
                }
            }

            MouseArea
            {
                anchors.fill: delegateColumn
                hoverEnabled: true

                onEntered:
                {
                    avatarminiframe.m_hovered = true
                }
                onExited:
                {
                    avatarminiframe.m_hovered = false
                }
            }

            Keys.onReturnPressed: function(event) {
                if (focus)
                {
                    let username = model.name

                    if (username != null)
                    {
                        let realname = model.realName
                        let pic = model.icon
                        let needspassword = model.needsPassword

                        if (needspassword)
                        {
                            userNameLabel.text = realname || username
                            avatar.source = pic

                            listView.currentIndex = index
                            pages.currentIndex = Main.LoginPage.Login
                        }
                        else
                        {
                            sddm.login(username, password.text, session.index)
                        }
                    }
                }
            }
        }
    }
    Loader {
        id: inputPanel
        state: "hidden"
        property bool active: false
        readonly property bool keyboardActive: item ? item.active : false
        anchors {
            left: parent.left
            right: parent.right
            bottom: background.bottom
            leftMargin: Kirigami.Units.gridUnit*12
            rightMargin: Kirigami.Units.gridUnit*12
        }
        function showHide() {
            setActive(!active)
        }
        function setActive(wanted) {
            active = wanted
            if (inputPanel.item) {
                inputPanel.item.activated = wanted
            }
            if (wanted) {
                password.forceActiveFocus()
                Qt.inputMethod.show()
            } else {
                Qt.inputMethod.hide()
            }
        }
        Component.onCompleted: {
            inputPanel.source = "SMOD/Aero7OnScreenKeyboard.qml"
        }
        onLoaded: {
            inputPanel.item.targetField = password
            inputPanel.item.submitCallback = function() {
                let index = listView.currentIndex
                let username = userModel.data(userModel.index(index, 0), Main.UserRoles.NameRole)
                if (username != null) {
                    sddm.login(username, password.text, session.index)
                }
            }
        }
        onKeyboardActiveChanged: {
            if (keyboardActive) {
                inputPanel.z = 99;
                Qt.inputMethod.show();
                active = true;
            } else {
                //inputPanel.item.activated = false;
                Qt.inputMethod.hide();
                active = false;
            }
        }

    }

    Plasma5Support.DataSource {
        id: accessibilityBackend
        engine: "executable"
        connectedSources: []
        property string action: ""
        property bool closeAfterApply: false

        function run(command, requestedAction, closeWhenDone) {
            action = requestedAction
            closeAfterApply = closeWhenDone
            connectSource(command)
        }

        function query() {
            run("/usr/bin/aero7-sddm-accessibility status", "status", false)
        }

        function apply(closeWhenDone) {
            const narrator = narratorCheck.checked ? "1" : "0"
            const sticky = stickyKeysCheck.checked ? "1" : "0"
            const filter = filterKeysCheck.checked ? "1" : "0"
            run("/usr/bin/aero7-sddm-accessibility apply " + narrator + " " + sticky + " " + filter,
                "apply", closeWhenDone)
        }

        onNewData: (sourceName, data) => {
            disconnectSource(sourceName)
            let reply
            try {
                reply = JSON.parse(data["stdout"])
            } catch (error) {
                accessStatus.text = i18nd("aerothemeplasma-sddm-theme", "Accessibility settings could not be read.")
                return
            }

            if (!reply.success) {
                accessStatus.text = reply.error || i18nd("aerothemeplasma-sddm-theme", "Accessibility settings could not be applied.")
                return
            }

            if (action === "status") {
                narratorCheck.checked = reply.narrator
                narratorCheck.enabled = reply.narratorAvailable
                stickyKeysCheck.checked = reply.stickyKeys
                filterKeysCheck.checked = reply.filterKeys
                accessStatus.text = reply.narratorAvailable ? ""
                    : i18nd("aerothemeplasma-sddm-theme", "Narrator is unavailable because Orca is not installed.")
            } else {
                accessDialog.applyLocalSettings()
                accessStatus.text = i18nd("aerothemeplasma-sddm-theme", "Accessibility settings applied.")
                if (closeAfterApply) {
                    accessDialog.close()
                }
            }
        }
    }

    StackLayout
    {
        id: pages
        anchors.fill: parent
        anchors.bottomMargin: inputPanel.active ? inputPanel.height : 0
        scale: root.magnifierEnabled ? 1.18 : 1.0

        onCurrentIndexChanged:
        {
            if (currentIndex == Main.LoginPage.SelectUser)
            {
                listView.forceActiveFocus()
            }
            else if (currentIndex == Main.LoginPage.Login)
            {
                password.forceActiveFocus()
            }
            else if (currentIndex == Main.LoginPage.LoginFailed)
            {
                dismissButton.forceActiveFocus()
            }
        }

    Plasma5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        property bool read: false
        property bool startupEnabled: !fileExists && config.boolValue("enableStartup");
        property bool fileExists: false
        onNewData: (sourceName, data) => {
            var stdout = data["stdout"]
            exited(stdout)
            disconnectSource(sourceName) // cmd finished
        }
        function exec(cmd, r) {
            executable.read = r;
            if (cmd) {
                connectSource(cmd)
            }
        }
        signal exited(string stdout)
    }

    Connections {
        target: executable
        function onExited(stdout) {
            if(executable.read) {
                if(stdout.trim() !== "") { // If the file exists, do not play Vista boot animation
                    executable.fileExists = true;
                } else {
                    executable.exec("touch /tmp/sddm.startup", false); // Create it to prevent multiple boot animations from happening
                    if(startupSound.playSound) {
                        startupSoundDelay.start()
                    }
                    if(executable.startupEnabled) seqanimation.start();

                }
            }
        }
    }

        // for testing failed login
        currentIndex: {
            return executable.startupEnabled ? Main.LoginPage.Startup : Main.LoginPage.SelectUser
        }

        function startSingleUserMode() {

            let singleusermode = userModel.count < 2 && !m_forceUserSelect

            if (singleusermode)
            {
                let index = 0;
                let username = userModel.data(userModel.index(index, 0), Main.UserRoles.NameRole)

                if (username != null)
                {
                    let userDisplayName = userModel.data(userModel.index(index, 0), Main.UserRoles.RealNameRole)
                    let userPicture = userModel.data(userModel.index(index, 0), Main.UserRoles.IconRole)

                    userNameLabel.text = userDisplayName || username
                    avatar.source = userPicture

                    pages.currentIndex = Main.LoginPage.Login
                    return true;
                }
            }
            pages.currentIndex = Main.LoginPage.SelectUser;
            return false;
        }

        Component.onCompleted:
        {
            executable.exec("ls /tmp/sddm.startup", true); // Check if sddm.startup exists
            startSingleUserMode();
        }
        Item
        {
            id: startupPage
        }

        Item
        {
            id: userlistpage
            // StackLayout owns this page's size and position.

            GridView
            {
                id: listView
                anchors.centerIn: parent

                anchors.verticalCenterOffset:
                {
                    if (count < 5)
                    {
                        return 72
                    }
                    else
                    {
                        return 0
                    }
                }

                width:
                {
                    if (count < 5)
                    {
                        return cellWidth * count
                    }
                    else
                    {
                        return cellWidth * 5
                    }
                }

                height:
                {
                    if (count > 5)
                    {
                        return 200 * 2
                    }
                    else
                    {
                        return 200
                    }
                }

                clip: true
                interactive: true
                keyNavigationEnabled: true
                keyNavigationWraps: false
                focus: true
                boundsBehavior: Flickable.StopAtBounds

                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

                cellWidth: 200
                cellHeight: 200

                model: userModel
                delegate: userDelegate
                currentIndex: userModel.lastIndex

                KeyNavigation.backtab: rebootButton
                KeyNavigation.tab: accessbutton

                MouseArea
                {
                    anchors.fill: parent

                    onClicked: (mouse) =>
                    {
                        let posInGridView = Qt.point(mouse.x, mouse.y)
                        let posInContentItem = mapToItem(listView.contentItem, posInGridView)
                        let index = listView.indexAt(posInContentItem.x, posInContentItem.y)

                        let username = userModel.data(userModel.index(index, 0), Main.UserRoles.NameRole)

                        if (username != null)
                        {
                            let realname = userModel.data(userModel.index(index, 0), Main.UserRoles.RealNameRole)
                            let pic = userModel.data(userModel.index(index, 0), Main.UserRoles.IconRole)
                            let needspassword = userModel.data(userModel.index(index, 0), Main.UserRoles.NeedsPasswordRole)

                            if (needspassword)
                            {
                                userNameLabel.text = realname || username;
                                avatar.source = pic

                                listView.currentIndex = index
                                pages.currentIndex = Main.LoginPage.Login
                            }
                            else
                            {
                                sddm.login(username, password.text, session.index)
                            }
                        }
                    }
                }
            }
        }

        Item
        {
            id: loginpage

            // Preserve the centered anchor origin without competing with child anchors.
            Item
            {
                id: mainColumn
                anchors.centerIn: parent

                Item
                {
                    id: userpic

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: -1
                    anchors.bottom: parent.verticalCenter
                    anchors.bottomMargin: m_biggerUserFrame ? -64 : -56

                    width: m_biggerUserFrame ? 238 : 190
                    height: width

                    Item
                    {
                        width: m_biggerUserFrame ? 158 : 126
                        height: width

                        anchors.centerIn: parent

                        Rectangle
                        {
                            id: mask

                            anchors.fill: parent
                            anchors.centerIn: parent
                            radius: 2
                            visible: false
                        }

                        LinearGradient {
                            id: gradient
                            anchors.fill: parent
                            anchors.centerIn: parent
                            z: -1
                            start: Qt.point(0,0)
                            end: Qt.point(gradient.width, gradient.height)
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#eeecee" }
                                GradientStop { position: 1.0; color: "#a39ea3" }
                            }
                        }
                        Image
                        {
                            id: avatar

                            fillMode: Image.PreserveAspectCrop

                            source: ""

                            property string defaultPic: "Assets/user.png"

                            onStatusChanged:
                            {
                                if (avatar.status == Image.Error)
                                {
                                    avatar.source = avatar.defaultPic;
                                }
                            }

                            anchors.fill: parent
                            anchors.centerIn: parent

                            layer.enabled: true
                            layer.effect: OpacityMask
                            {
                                maskSource: mask
                            }
                        }
                    }

                    Image
                    {
                        source: m_biggerUserFrame ? "Assets/12238.png" : "Assets/12223.png"
                        anchors.fill: parent
                        anchors.centerIn: parent
                    }
                }
                
                QQC2.Label
                {
                    id: userNameLabel

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: userpic.bottom

                    text: ""
                    color: "white"

                    font.pixelSize: 23
                    font.kerning: false
                    renderType: Text.NativeRendering
                    font.hintingPreference: Font.PreferVerticalHinting

                    layer.enabled: true
                    layer.effect: DropShadow
                    {
                        verticalOffset: 1
                        color: "#ef000000"
                        radius: 9
                        samples: 80
                    }
                }

                Item
                {
                    id: loginbox

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: userNameLabel.bottom
                    anchors.topMargin: 8
                    
                    width: 225
                    height: 24


                    QQC2.TextField {
                        id: password

                        anchors.fill: parent

                        bottomPadding: 0
                        topPadding: 0
                        
                        font.pointSize: 9

                        placeholderTextColor: "#555"
                        background: Image {
                            source:
                            {
                                if (password.focus) return "Assets/input-focus.png"
                                if (password.hovered) return "Assets/input-hover.png"
                                return "Assets/input.png"
                            }
                        }

                        placeholderText: i18nd("aerothemeplasma-sddm-theme", "Password")
                        selectByMouse: true
                        echoMode : TextInput.Password
                        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText

                        KeyNavigation.backtab: switchLayoutButton
                        KeyNavigation.tab: loginButton
                        KeyNavigation.down:
                        {
                            if (switchuser.enabled)
                            {
                                return switchuser
                            }
                            else
                            {
                                return accessbutton
                            }
                        }

                        Keys.onPressed : (event) => {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            {
                                let index = listView.currentIndex
                                let username = userModel.data(userModel.index(index, 0), Main.UserRoles.NameRole)

                                if (username != null)
                                {
                                    sddm.login(username, password.text, session.index)
                                }

                                event.accepted = true
                            }
                            else if (
                                event.matches(StandardKey.Undo) ||
                                event.matches(StandardKey.Redo) ||
                                event.matches(StandardKey.Cut) ||
                                event.matches(StandardKey.Copy) ||
                                event.matches(StandardKey.Paste)
                                )
                            {
                                // disable these events
                                event.accepted = true
                            }
                        }
                    }
                    RowLayout {
                        spacing: 2
                        anchors.top: password.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 2
                        visible: keyboard.capsLock && password.visible
                        Image {
                            id: iconSmall
                            width: Kirigami.Units.iconSizes.small;
                            height: Kirigami.Units.iconSizes.small;

                            source: "./Assets/dialog-warning.png"
                            sourceSize.width: iconSmall.width
                            sourceSize.height: iconSmall.height
                        }
                        QQC2.Label {
                            id: notificationsLabel
                            font.pointSize: 9
                            text: i18nd("plasma_lookandfeel_org.kde.lookandfeel", "Caps Lock is on");
                            width: implicitWidth
                            renderType: Text.NativeRendering
                            color: "white"
                        }
                    }

                    QQC2.Button
                    {
                        id: loginButton

                        anchors.left: password.right
                        anchors.verticalCenter: password.verticalCenter
                        anchors.verticalCenterOffset: -1
                        anchors.leftMargin: 4

                        width: 30
                        height: 30

                        background: Image
                        {
                            source: loginButton.pressed ? "Assets/gopressed.png" : (loginButton.focus || loginButton.hovered ? "Assets/gohover.png" : "Assets/go.png")
                        }

                        onClicked:
                        {
                            let index = listView.currentIndex
                            let username = userModel.data(userModel.index(index, 0), Main.UserRoles.NameRole)

                            if (username != null)
                            {
                                sddm.login(username, password.text, session.index)
                            }
                        }

                        KeyNavigation.backtab: password
                        KeyNavigation.tab:
                        {
                            if (switchuser.enabled)
                            {
                                return switchuser
                            }
                            else
                            {
                                return accessbutton
                            }
                        }

                        KeyNavigation.down:
                        {
                            if (switchuser.enabled)
                            {
                                return switchuser
                            }
                            else
                            {
                                return accessbutton
                            }
                        }
                    }
                }
                QQC2.Button
            {
                id: switchuser

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: userpic.bottom
                anchors.topMargin: 124
                //anchors.bottom: parent.bottom

                //anchors.bottomMargin: 332

                width: 108
                height: 28

                background: Image
                {
                    source:
                    {
                        if (switchuser.pressed) return "Assets/switch-user-button-active.png"
                        if (switchuser.hovered && switchuser.focus) return "Assets/switch-user-button-hover-focus.png"
                        if (switchuser.hovered && !switchuser.focus) return "Assets/switch-user-button-hover.png"
                        if (!switchuser.hovered && switchuser.focus) return "Assets/switch-user-button-focus.png"
                        return "Assets/switch-user-button.png"
                    }
                }

                contentItem: Text
                {
                    text: i18nd("aerothemeplasma-sddm-theme", "Switch User")
                    color: "white"

                    //font.family: mainfont.name
                    font.pointSize: 11
                    font.kerning: false
                    renderType: Text.NativeRendering
                    font.hintingPreference: Font.PreferFullHinting


                    horizontalAlignment : Text.AlignHCenter
                    verticalAlignment : Text.AlignVCenter

                    bottomPadding: 3
                }

                KeyNavigation.backtab: password
                KeyNavigation.up: password
                KeyNavigation.tab: accessbutton
                KeyNavigation.down: accessbutton

                onClicked:
                {
                    password.text = ""
                    pages.currentIndex = Main.LoginPage.SelectUser
                }

                Keys.onReturnPressed: function(event) {
                    clicked()
                    event.accepted = true
                }
            }
            }


        }


        Item
        {
            id: loginfailedpage

            Keys.onEscapePressed:
            {
                pages.currentIndex = Main.LoginPage.Login
            }

            Image
            {
                id: currentMessageIcon

                anchors.right: currentMessage.left
                anchors.verticalCenter: currentMessage.verticalCenter

                anchors.verticalCenterOffset: -2
                anchors.rightMargin: 11

                source: "Assets/dialog-error.png"
                width: 32
                height: 32

                focus: false

                smooth: false
            }

            QQC2.Label
            {
                id: currentMessage

                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter

                anchors.verticalCenterOffset: 126
                anchors.horizontalCenterOffset: 14

                text: i18nd("aerothemeplasma-sddm-theme", "The user name or password is incorrect.")
                renderType: Text.NativeRendering
                Layout.alignment: Qt.AlignHCenter
                font.pointSize: 9

                focus: false

                width: implicitWidth
                color: "white"
                horizontalAlignment: Text.AlignCenter
                layer.enabled: true
                layer.effect: DropShadow {
                    horizontalOffset: 0
                    verticalOffset: 1
                    radius: 6
                    samples: 14
                    spread: 0.0001
                    color: "#bf000000"
                }
            }

            QQC2.Button
            {
                id: dismissButton

                anchors.top: currentMessage.bottom
                anchors.horizontalCenter: parent.horizontalCenter

                anchors.topMargin: 46

                width: 93
                height: 28

                background: Image
                {
                    source:
                    {
                        if (dismissButton.pressed) return "Assets/button-active.png"
                        if (dismissButton.hovered && dismissButton.focus) return "Assets/button-hover-focus.png"
                        if (dismissButton.hovered && !dismissButton.focus) return "Assets/button-hover.png"
                        if (!dismissButton.hovered && dismissButton.focus) return "Assets/button-focus.png"
                        return "Assets/button.png"
                    }
                }

                contentItem: Text
                {
                    text: i18nd("aerothemeplasma-sddm-theme", "OK")
                    color: "white"

                    //font.family: mainfont.name
                    font.pointSize: 11

                    horizontalAlignment : Text.AlignHCenter
                    verticalAlignment : Text.AlignVCenter
                    renderType: Text.NativeRendering

                    bottomPadding: 3
                    rightPadding: 1
                }

                onClicked:
                {
                    pages.currentIndex = Main.LoginPage.Login
                }

                Keys.onReturnPressed: function(event) {
                    clicked()
                    event.accepted = true
                }
            }
        }
    }

    Image
    {
        id: branding
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 23
        anchors.horizontalCenter: parent.horizontalCenter
        source: Qt.resolvedUrl("Assets/aero7-branding-r3.png")
        // The source canvas preserves the historical 350x50 theme slot, but
        // the actual Aero7 wordmark occupies its first 225 pixels. Crop that
        // transparent tail so the visible logo, rather than its canvas, is
        // centered on every display.
        sourceClipRect: Qt.rect(0, 0, 225, 50)
        width: Math.min(225, parent.width - 40)
        height: width * 50 / 225
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: pages.currentIndex != Main.LoginPage.Startup
    }
    SMOD.GenericButton {
            id: switchLayoutButton

            readonly property int currentIndex: keyboard.currentLayout
            readonly property var currentLayout: keyboard.layouts[currentIndex]

            anchors {
                top: parent.top
                topMargin: 5
                left: parent.left
                leftMargin: 7
            }
            implicitWidth: 35
            implicitHeight: 28
            label.font.pointSize: 9
            label.font.capitalization: Font.AllUppercase
            Accessible.description: i18ndc("plasma_lookandfeel_org.kde.lookandfeel", "Button to change keyboard layout", "Switch layout")
            KeyNavigation.backtab: rebootButton
            KeyNavigation.tab: pages.currentIndex === Main.LoginPage.SelectUser ? listView : password
            KeyNavigation.down: pages.currentIndex === Main.LoginPage.SelectUser ? listView : password

            text: currentLayout ? currentLayout.shortName : ""
            onClicked: {
                const count = keyboard.layouts.length
                if (count > 0) {
                    keyboard.currentLayout = currentIndex >= 0 && currentIndex < count
                        ? (currentIndex + 1) % count : 0
                }
            }

            visible: keyboard.layouts.length > 1 && pages.currentIndex != Main.LoginPage.Startup
        }

    Item {
        id: accessDialog
        // Keep the Windows-style dialog outside the magnified login-page
        // transform.  Without this explicit visual parent, enabling Magnifier
        // reflows QQC2 controls inside the scaled StackLayout and collapses
        // their labels to single characters.
        parent: root
        anchors.fill: parent
        z: 200
        visible: false
        focus: visible

        property bool pendingHighContrast: false
        property bool pendingMagnifier: false
        property bool pendingKeyboard: false

        function open() {
            pendingHighContrast = root.highContrastEnabled
            pendingMagnifier = root.magnifierEnabled
            pendingKeyboard = inputPanel.active
            desktopEnvironmentCombo.index = session.index
            accessStatus.text = i18nd("aerothemeplasma-sddm-theme", "Reading accessibility settings…")
            visible = true
            forceActiveFocus()
            narratorCheck.forceActiveFocus()
            accessibilityBackend.query()
        }

        function close() {
            visible = false
            accessbutton.checked = false
            if (inputPanel.active) {
                // Closing the dialog used to move focus away from the password
                // field and make Qt hide the keyboard that the user had just
                // enabled. Restore both after the checked-state callback has
                // completed so the keyboard remains usable for sign-in.
                password.forceActiveFocus()
                Qt.callLater(function() { inputPanel.setActive(true) })
            } else {
                accessbutton.forceActiveFocus()
            }
        }

        function applyLocalSettings() {
            root.highContrastEnabled = pendingHighContrast
            root.magnifierEnabled = pendingMagnifier
            inputPanel.setActive(pendingKeyboard)
            session.index = desktopEnvironmentCombo.index
            session.valueChanged(session.index)
        }

        Keys.onEscapePressed: (event) => {
            close()
            event.accepted = true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: accessDialog.close()
        }

        Rectangle {
            id: accessDialogWindow
            width: Math.min(560, parent.width - 40)
            height: Math.min(575, parent.height - 40)
            anchors.centerIn: parent
            color: "#f6f6f6"
            border.color: "#586f84"
            border.width: 1
            radius: 2

            MouseArea {
                anchors.fill: parent
                onClicked: (mouse) => mouse.accepted = true
            }

            Rectangle {
                id: accessTitleBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 34
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#e7f3ff" }
                    GradientStop { position: 1.0; color: "#a9c7e4" }
                }
                border.color: "#7d9db9"

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    source: "Assets/12213.png"
                    smooth: false
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 36
                    anchors.verticalCenter: parent.verticalCenter
                    text: i18nd("aerothemeplasma-sddm-theme", "Ease of Access")
                    color: "#15202b"
                    font.pixelSize: 13
                    renderType: Text.NativeRendering
                }

                QQC2.Button {
                    width: 46
                    height: 25
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×"
                    onClicked: accessDialog.close()
                    Accessible.name: i18nd("aerothemeplasma-sddm-theme", "Close")
                }
            }

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: accessTitleBar.bottom
                anchors.bottom: buttonArea.top
                anchors.margins: 22
                spacing: 6

                Text {
                    text: i18nd("aerothemeplasma-sddm-theme", "Make your computer easier to use")
                    color: "#003399"
                    font.pixelSize: 18
                    Layout.fillWidth: true
                    renderType: Text.NativeRendering
                }

                Text {
                    text: i18nd("aerothemeplasma-sddm-theme", "Press SPACEBAR to select the highlighted option.")
                    color: "#202020"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    renderType: Text.NativeRendering
                }

                QQC2.CheckBox {
                    id: narratorCheck
                    text: i18nd("aerothemeplasma-sddm-theme", "Hear text on screen read aloud (Narrator)")
                    Layout.fillWidth: true
                    KeyNavigation.tab: magnifierCheck
                }

                QQC2.CheckBox {
                    id: magnifierCheck
                    text: i18nd("aerothemeplasma-sddm-theme", "Make items on the screen larger (Magnifier)")
                    checked: accessDialog.pendingMagnifier
                    onToggled: accessDialog.pendingMagnifier = checked
                    Layout.fillWidth: true
                    KeyNavigation.tab: highContrastCheck
                }

                QQC2.CheckBox {
                    id: highContrastCheck
                    text: i18nd("aerothemeplasma-sddm-theme", "See more contrast in colors (High Contrast)")
                    checked: accessDialog.pendingHighContrast
                    onToggled: accessDialog.pendingHighContrast = checked
                    Layout.fillWidth: true
                    KeyNavigation.tab: onScreenKeyboardCheck
                }

                QQC2.CheckBox {
                    id: onScreenKeyboardCheck
                    text: i18nd("aerothemeplasma-sddm-theme", "Type without the keyboard (On-Screen Keyboard)")
                    checked: accessDialog.pendingKeyboard
                    onToggled: accessDialog.pendingKeyboard = checked
                    Layout.fillWidth: true
                    KeyNavigation.tab: stickyKeysCheck
                }

                QQC2.CheckBox {
                    id: stickyKeysCheck
                    text: i18nd("aerothemeplasma-sddm-theme", "Press keyboard shortcuts one key at a time (Sticky Keys)")
                    Layout.fillWidth: true
                    KeyNavigation.tab: filterKeysCheck
                }

                QQC2.CheckBox {
                    id: filterKeysCheck
                    text: i18nd("aerothemeplasma-sddm-theme", "Ignore brief or repeated keystrokes (Filter Keys)")
                    Layout.fillWidth: true
                    KeyNavigation.tab: desktopEnvironmentCombo
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#b7b7b7"
                    Layout.topMargin: 4
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: i18nd("aerothemeplasma-sddm-theme", "Desktop environment:")
                        color: "#202020"
                        font.pixelSize: 12
                        renderType: Text.NativeRendering
                    }
                    SMOD.ComboBox {
                        id: desktopEnvironmentCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        model: sessionModel
                        index: session.index
                        arrowIcon: Qt.resolvedUrl("Assets/power-glyph-arrow.png")
                        KeyNavigation.tab: accessOkButton
                    }
                }

                Text {
                    id: accessStatus
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    wrapMode: Text.WordWrap
                    color: "#7b1f1f"
                    font.pixelSize: 11
                    renderType: Text.NativeRendering
                }
            }

            Rectangle {
                id: buttonArea
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 52
                color: "#ececec"
                border.color: "#d2d2d2"

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    QQC2.Button {
                        id: accessOkButton
                        width: 82
                        text: i18nd("aerothemeplasma-sddm-theme", "OK")
                        onClicked: accessibilityBackend.apply(true)
                        KeyNavigation.tab: accessCancelButton
                    }
                    QQC2.Button {
                        id: accessCancelButton
                        width: 82
                        text: i18nd("aerothemeplasma-sddm-theme", "Cancel")
                        onClicked: accessDialog.close()
                        KeyNavigation.tab: accessApplyButton
                    }
                    QQC2.Button {
                        id: accessApplyButton
                        width: 82
                        text: i18nd("aerothemeplasma-sddm-theme", "Apply")
                        onClicked: accessibilityBackend.apply(false)
                        KeyNavigation.tab: narratorCheck
                    }
                }
            }
        }
    }

    QQC2.CheckBox
    {
        id: accessbutton

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 34
        anchors.leftMargin: 34

        width: 38
        height: 28

        visible: pages.currentIndex != Main.LoginPage.LoginFailed && pages.currentIndex != Main.LoginPage.Startup

        indicator.width: 0
        indicator.height: 0
        Accessible.name: i18nd("aerothemeplasma-sddm-theme", "Ease of Access and desktop environment")

        background: Image
        {
            source:
            {
                if (accessbutton.pressed) return "Assets/access-button-active.png"
                if (accessbutton.hovered && accessbutton.focus) return "Assets/access-button-hover-focus.png"
                if (accessbutton.hovered && !accessbutton.focus) return "Assets/access-button-hover.png"
                if (!accessbutton.hovered && accessbutton.focus) return "Assets/access-button-focus.png"
                return "Assets/access-button.png"
            }
        }

        contentItem: Item
        {
            anchors.centerIn: parent

            width: 24
            height: 24

            Image
            {
                anchors.centerIn: parent
                width: 24
                height: 24
                source: "Assets/12213.png"
                smooth: false
            }
        }

        enabled: !accessDialog.visible
        onToggled:
        {
            if (checked) accessDialog.open()
            else accessDialog.close()
        }

        Keys.onReturnPressed: function(event) {
            clicked()
            event.accepted = true
        }

        KeyNavigation.backtab:
        {
            if (pages.currentIndex === Main.LoginPage.SelectUser)
            {
                return listView
            }
            else if (switchuser.enabled)
            {
                return switchuser
            }

            return password
        }

        KeyNavigation.up:
        {
            if (pages.currentIndex === Main.LoginPage.Login && switchuser.enabled)
            {
                return switchuser
            }

            return accessbutton
        }

        KeyNavigation.tab: shutdownButton
        KeyNavigation.right: shutdownButton
        Item
        {
        anchors.bottom: parent.top
        anchors.left: parent.left
        anchors.bottomMargin: -32

        visible: pages.currentIndex != Main.LoginPage.LoginFailed
        enabled: visible

        width: 128
        height: 64

        SMOD.Menu {
            id: session
            visible: false
            x: 0
            y: -session.height + accessbutton.height + session.verticalPadding
            index: sessionModel.lastIndex

            function changeVal() {
                session.valueChanged(this.index);
                session.index = this.index;
            }
            function isIndex() {
                return session.index === this.index
            }
            Component.onCompleted: {
                var entries = [];
                for(var i = 0; i < sessionModel.count; i++) {
                    const NameRole = sessionModel.KItemModels.KRoleNames.role("name");
                    const name = sessionModel.data(sessionModel.index(i, 0), NameRole);
                    entries.push({name: name, index: i});
                }

                function sessionRank(name) {
                    if(name === "Aero7 Desktop (Safe Mode)") return 0;
                    if(name === "Aero7 Desktop") return 1;
                    if(name.indexOf("AeroThemePlasma") === 0) return 2;
                    if(name.indexOf("Plasma") === 0) return 3;
                    return 100;
                }

                entries.sort(function(left, right) {
                    var rankDifference = sessionRank(left.name) - sessionRank(right.name);
                    return rankDifference !== 0 ? rankDifference : left.index - right.index;
                });

                for(var entryIndex = 0; entryIndex < entries.length; entryIndex++) {
                    var entry = entries[entryIndex];
                    var menuitem = createMenuItem();
                    menuitem.text = entry.name;
                    menuitem.index = entry.index;
                    var func = isIndex.bind({index: entry.index});
                    menuitem.checkable = true;
                    menuitem.checked = Qt.binding(func);
                    menuitem.triggered.connect(changeVal.bind({index: entry.index}));
                    session.addAction(menuitem);
                }

                session.addItem(createMenuSeparator());
                var keyboardItem = createMenuItem();
                keyboardItem.text = i18nd("aerothemeplasma-sddm-theme", "On-Screen Keyboard");
                keyboardItem.checkable = false;
                keyboardItem.icon.source = Qt.resolvedUrl("Assets/keyboard.png");
                keyboardItem.triggered.connect(function() {
                    password.forceActiveFocus();
                    inputPanel.showHide();
                });
                session.addAction(keyboardItem);
            }

        }
    }
    }

    Item
    {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 34
        anchors.rightMargin: 30

        visible: pages.currentIndex != Main.LoginPage.LoginFailed && pages.currentIndex != Main.LoginPage.Startup
        enabled: pages.currentIndex != Main.LoginPage.Startup

        width: 62
        height: 28

        QQC2.Button
        {
            id: shutdownButton

            anchors.bottom: parent.bottom
            anchors.left: parent.left

            width: 38
            height: 28

            background: Image
            {
                source:
                {
                    if (shutdownButton.pressed) return "Assets/power-active.png"
                    if (shutdownButton.hovered && shutdownButton.focus) return "Assets/power-hover-focus.png"
                    if (shutdownButton.hovered && !shutdownButton.focus) return "Assets/power-hover.png"
                    if (!shutdownButton.hovered && shutdownButton.focus) return "Assets/power-focus.png"
                    return "Assets/power.png"
                }
            }

            contentItem: Item
            {
                anchors.centerIn: parent

                width: 24
                height: 24

                Image
                {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    source: "Assets/power-glyph.png"
                    smooth: false
                }
            }

            onClicked : sddm.powerOff()

            Keys.onReturnPressed: function(event) {
                clicked()
                event.accepted = true
            }

            KeyNavigation.backtab: accessbutton
            KeyNavigation.left: accessbutton
            KeyNavigation.tab: rebootButton
            KeyNavigation.right: rebootButton
            KeyNavigation.up:
            {
                if (pages.currentIndex === Main.LoginPage.Login && switchuser.enabled)
                {
                    return switchuser
                }

                return shutdownButton
            }

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            QQC2.ToolTip.text: i18nd("aerothemeplasma-sddm-theme", "Shut down")
        }

        QQC2.Button
        {
            id: rebootButton

            anchors.bottom: parent.bottom
            anchors.left: shutdownButton.right

            visible: pages.currentIndex != Main.LoginPage.Startup

            width: 20
            height: 28

            background: Image
            {
                source:
                {
                    if (rebootButton.pressed) return "Assets/12301.png"
                    if (rebootButton.hovered && rebootButton.focus) return "Assets/12298.png"
                    if (rebootButton.hovered && !rebootButton.focus) return "Assets/12300.png"
                    if (!rebootButton.hovered && rebootButton.focus) return "Assets/12299.png"
                    return "Assets/12302.png"
                }
            }

            contentItem: Item
            {
                anchors.centerIn: parent

                width: 9
                height: 6

                Image
                {
                    anchors.centerIn: parent
                    width: 9
                    height: 6
                    source: "Assets/power-glyph-arrow.png"
                    smooth: false
                }
            }
            enabled: !powerMenu.visible
            onClicked: {
                if(powerMenu.visible) powerMenu.close();
                else powerMenu.open();

            }

            SMOD.Menu {
                id: powerMenu
                x: -powerMenu.width + parent.width //-parent.width + powerMenu.horizontalPadding
                y: -powerMenu.height //-powerMenu.height + rebootButton.height //+ powerMenu.verticalPadding

                Component.onCompleted: {
                    var menuitem = powerMenu.createMenuItem();
                    menuitem.text = i18nd("aerothemeplasma-sddm-theme", "Restart");
                    menuitem.triggered.connect(() => { sddm.reboot() });
                    powerMenu.addAction(menuitem);
                    powerMenu.addItem(powerMenu.createMenuSeparator());

                    if(sddm.canSuspend) {
                        menuitem = powerMenu.createMenuItem();
                        menuitem.text = i18nd("aerothemeplasma-sddm-theme", "Sleep");
                        menuitem.triggered.connect(() => { sddm.suspend() });
                        powerMenu.addAction(menuitem);
                    }
                    if(sddm.canHibernate) {
                        menuitem = powerMenu.createMenuItem();
                        menuitem.text = i18nd("aerothemeplasma-sddm-theme", "Hibernate");
                        menuitem.triggered.connect(() => { sddm.hibernate() });
                        powerMenu.addAction(menuitem);
                    }
                    if(sddm.canHybridSleep) {
                        menuitem = powerMenu.createMenuItem();
                        menuitem.text = i18nd("aerothemeplasma-sddm-theme", "Hybrid Sleep");
                        menuitem.triggered.connect(() => { sddm.hybridSleep() });
                        powerMenu.addAction(menuitem);
                    }
                    menuitem = powerMenu.createMenuItem();
                    menuitem.text = i18nd("aerothemeplasma-sddm-theme", "Shut down");
                    menuitem.triggered.connect(() => { sddm.powerOff() });
                    powerMenu.addAction(menuitem);
                }

            }
            Keys.onReturnPressed: event => {
                clicked()
                event.accepted = true
            }

            KeyNavigation.backtab: shutdownButton
            KeyNavigation.left: shutdownButton
            KeyNavigation.up:
            {
                if (pages.currentIndex === Main.LoginPage.Login && switchuser.enabled)
                {
                    return switchuser
                }

                return rebootButton
            }

            KeyNavigation.tab: switchLayoutButton
        }
    }
    Item
    {
        id: startuppage

        z: 99
        anchors.fill: parent
        SequentialAnimation {
            id: seqanimation
            NumberAnimation { target: startuppage; property: "opacity"; to: 1;   duration: 220; easing.type: Easing.Linear }
            NumberAnimation { target: startupimage; property: "opacity"; to: 1;  duration: 650; easing.type: Easing.Linear }
            NumberAnimation { target: startupimage; property: "opacity"; to: 1;  duration: 200; easing.type: Easing.Linear }
            NumberAnimation { target: startupimage2; property: "opacity"; to: 1; duration: 650; easing.type: Easing.Linear }
            NumberAnimation { target: startupimage3; property: "opacity"; to: 1; duration: 650; easing.type: Easing.Linear }
            NumberAnimation { target: startupimage4; property: "opacity"; to: 1; duration: 650; easing.type: Easing.Linear }
            NumberAnimation { target: startupimage4; property: "opacity"; to: 1; duration: 200; easing.type: Easing.Linear }
            ParallelAnimation {
                NumberAnimation { target: startupimage2; property: "opacity"; to: 0;  duration: 650; easing.type: Easing.OutQuad }
                NumberAnimation { target: startupimage3; property: "opacity"; to: 0;  duration: 650; easing.type: Easing.OutQuad }
                NumberAnimation { target: startupimage4; property: "opacity"; to: 0;  duration: 650; easing.type: Easing.OutQuad }
                NumberAnimation { target: startupimage; property: "opacity"; to: 0;  duration: 950; easing.type:  Easing.OutQuad }
            }
            ScriptAction { script: { pages.startSingleUserMode(); } }
            NumberAnimation { target: startuppage; property: "opacity"; to: 0; duration: 650; easing.type: Easing.Linear }
            NumberAnimation { target: startupanimation; property: "opacity"; to: 0; duration: 650; easing.type: Easing.OutQuad }
            PropertyAction { target: executable; property: "startupEnabled"; value: false }
        }
        visible: executable.startupEnabled
        opacity: 1
        Rectangle
        {
            color: "black"
            anchors.fill: parent
        }

        Item
        {
            id: startupanimation
            property int progress: 0
            anchors.centerIn: parent

            Image
            {
                id: startupimage
                anchors.centerIn: parent
                source: Qt.resolvedUrl("./Assets/17000")
                opacity: 0
            }

            Image
            {
                id: startupimage2
                anchors.centerIn: parent
                source: Qt.resolvedUrl("./Assets/17001")
                opacity: 0
            }

            Image
            {
                id: startupimage3
                anchors.centerIn: parent
                source: Qt.resolvedUrl("./Assets/17002")
                opacity: 0
            }

            Image
            {
                id: startupimage4
                anchors.centerIn: parent
                source: Qt.resolvedUrl("./Assets/17003")
                opacity: 0
            }

        }

        // to hide the cursor
        MouseArea
        {
            anchors.fill: parent
            cursorShape: Qt.BlankCursor
        }
    }

}
