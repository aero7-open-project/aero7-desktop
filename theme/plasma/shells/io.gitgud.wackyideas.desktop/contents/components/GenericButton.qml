import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg

import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras

Control {
    id: genericButton

    signal clicked()

    property string text: ""
    property string iconSource: ""
    property string iconImage: ""
    property int    iconSize: Kirigami.Units.iconSizes.smallMedium
    property alias  label: btnLabel
    property bool   checked: false

    implicitWidth: {
        if(text !== "") return label.implicitWidth + Kirigami.Units.largeSpacing * 2;
        else if(iconSource != "") return iconSize + ((Kirigami.Units.largeSpacing * 2) - 2);
        else return btnImage.width + Kirigami.Units.largeSpacing;
    }
    implicitHeight: {
        if(text !== "") return label.implicitHeight + (Kirigami.Units.largeSpacing - 1);
        else if(iconSource != "") return iconSize + Kirigami.Units.smallSpacing;
        else return btnImage.height + Kirigami.Units.smallSpacing;
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => {
        if(event.key == Qt.Key_Return) {
            genericButton.clicked();
        }
    }

    KSvg.FrameSvgItem {
        id: texture

        anchors.fill: parent

        imagePath: Qt.resolvedUrl("../images/button.svg");
        prefix: {
            var result = "";

            if(genericButton.focus) result = "focus-";
            if(buttonMA.containsPress || root.checked) result = "pressed";
            else if(buttonMA.containsMouse) result += "hover";
            else result += "normal";

            return result;
        }
    }

    Kirigami.Icon {
        id: btnIcon

        anchors.centerIn: genericButton

        width: genericButton.iconSize
        height: width

        source: genericButton.iconSource

        visible: genericButton.iconSource !== ""
    }

    Image {
        id: btnImage

        anchors.centerIn: genericButton

        source: genericButton.iconImage

        visible: genericButton.iconImage !== ""
    }

    PlasmaComponents.Label {
        id: btnLabel

        anchors.fill: parent

        text: genericButton.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        renderType: Text.NativeRendering
        font.hintingPreference: Font.PreferFullHinting
        font.kerning: false

        visible: genericButton.text !== ""

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "black"
            shadowOpacity: 1.0
            shadowBlur: 0.35
            shadowVerticalOffset: 1
        }
    }

    MouseArea {
        id: buttonMA

        anchors.fill: parent

        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: genericButton.clicked();
    }
}
