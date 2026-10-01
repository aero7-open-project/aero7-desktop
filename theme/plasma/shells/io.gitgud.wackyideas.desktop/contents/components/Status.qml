/*
    SPDX-FileCopyrightText: 2016 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects

RowLayout {
    id: root

    property alias statusText: welcomeLbl.text
    property int   spinnum: 0
    property bool  speen

    spacing: 8

    Timer {
        id: spinner
        running: root.speen
        repeat: true
        onTriggered: root.spinnum = (root.spinnum + 1) % 17;
        interval: 53
    }

    Image {
        id: loadingspinner
        source: "../images/spinner/100/spin" + root.spinnum + ".png"
    }

    Label {
        id: welcomeLbl

        color: "#FFFFFF"
        font.pointSize: 18
        renderType: Text.NativeRendering
        font.hintingPreference: Font.PreferFullHinting
        font.kerning: false

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "black"
            shadowOpacity: 1.0
            shadowBlur: 0.35
            shadowVerticalOffset: 1
        }
    }
}
