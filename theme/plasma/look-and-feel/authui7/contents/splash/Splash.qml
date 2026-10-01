/*
 *   Copyright 2014 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2,
 *   or (at your option) any later version, as published by the Free
 *   Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Templates
import org.kde.plasma.core as PlasmaCore
import QtMultimedia
import "../components"

import aeroshell.utils as AeroShellUtils

Item {
    id: root
    z: -9

    property int stage
    // ksplashqml keeps the overlay alive for this long after the desktop
    // stage, so fading this transparent item reveals the real desktop.
    property int splashExitDelayMs: 500

    onStageChanged: {
        if (stage >= 6) {
            opacity = 0;
        }
    }
    /*MediaPlayer {
        id: lockSuccess
        source: Qt.resolvedUrl("../sounds/lockSuccess.ogg");
        audioOutput: AudioOutput {}
    }*/

    Rectangle {
        color: "#1D5F7A"
        anchors.fill: parent
    }

    AeroShellUtils.SDDM { id: sddm }

    Image {
        id: bgtexture
        source: sddm.currentBackground
        anchors.fill: parent
    }

    Status {
        id: statusText
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -36
        statusText: i18nd("okular", "Welcome")
        speen: true
    }

    RowLayout {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        height: 96
        Rectangle { Layout.fillWidth: true }
        Image {
            id: watermark
            source: "../images/watermark.png"
        }
        Rectangle { Layout.fillWidth: true }
    }

    Behavior on opacity {
        NumberAnimation { duration: root.splashExitDelayMs; easing.type: Easing.InOutQuad }
    }
}
