import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.ksvg as KSvg

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore

MouseArea {
    id: groupThumbnails

    property QtObject root

    readonly property bool isList: (196 * thumbnailModel.count) > tasks.availableScreenRect.width
    readonly property bool containsDrag: root.containsDrag
    readonly property bool isOverflowing: thumbnailList.listHeight > tasks.availableScreenRect.height

    readonly property alias thumbnailHeight: thumbnailList.maxThumbnailHeight

    implicitWidth: (isList ? thumbnailList.maxThumbnailWidth : thumbnailList.listWidth)
    implicitHeight: (isList ? (isOverflowing ? tasks.availableScreenRect.height : thumbnailList.listHeight) : thumbnailList.maxThumbnailHeight) + (isOverflowing ? 0 : scrollView.anchors.topMargin + scrollView.anchors.bottomMargin)

    hoverEnabled: true
    propagateComposedEvents: true

    DelegateModel {
        id: thumbnailModel

        model: tasksModel
        rootIndex: tasksModel.makeModelIndex(root.taskIndex)
        delegate: WindowThumbnail {
            isGroupDelegate: true
            root: groupThumbnails.root
        }
    }
    DelegateModel {
        id: listModel

        model: tasksModel
        rootIndex: tasksModel.makeModelIndex(root.taskIndex)
        delegate: WindowListDelegate {
            root: groupThumbnails.root
        }
    }

    QQC2.ScrollView {
        id: scrollView

        anchors.fill: parent
        anchors.bottomMargin: !isList ? 0 : Kirigami.Units.smallSpacing*2
        anchors.topMargin: !isList ? 0 : Kirigami.Units.smallSpacing*2
        anchors.leftMargin: 0
        anchors.rightMargin: 0

        rightPadding: QQC2.ScrollBar.vertical.visible ? QQC2.ScrollBar.vertical.width : 0

        ListView {
            id: thumbnailList

            // Store measurements, not a binding to a delegate whose assigned
            // geometry depends on this group. Recompute from all live items so
            // shrinking/removing the old maximum also reduces the group size.
            property real maxThumbnailWidth: 0
            property real maxThumbnailHeight: 0

            property int listWidth: contentWidth == 0 ? 196 : contentWidth
            property int listHeight: contentHeight == 0 ? 142 : contentHeight

            function updateMaxSize() {
                let maximumWidth = 0;
                let maximumHeight = 0;
                for (let i = 0; i < count; ++i) {
                    const item = itemAtIndex(i);
                    if (item) {
                        maximumWidth = Math.max(maximumWidth, item.implicitWidth);
                        maximumHeight = Math.max(maximumHeight, item.implicitHeight);
                    }
                }
                maxThumbnailWidth = maximumWidth;
                maxThumbnailHeight = maximumHeight;
            }

            interactive: false
            spacing: -Kirigami.Units.smallSpacing*4 + 2
            orientation: !isList ? ListView.Horizontal : ListView.Vertical
            model: !isList ? thumbnailModel : listModel
            clip: true

            // Let delegates/layouts settle, including transitions to one or
            // zero rows and switches between thumbnails and the overflow list.
            onCountChanged: updateDelayTimer.restart()
            onModelChanged: updateDelayTimer.restart()

            Timer {
                id: updateDelayTimer

                interval: 15
                repeat: false
                triggeredOnStart: false
                onTriggered: thumbnailList.updateMaxSize();
            }
        }
    }
}
