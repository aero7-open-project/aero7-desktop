import QtQuick
import org.kde.plasma.extras as PlasmaExtras

PlasmaExtras.Menu {
    id: menu

    required property var runAction
    property bool taskbarLocked: true

    PlasmaExtras.MenuItem {
        text: i18n("Cascade windows")
        icon: "window-duplicate"
        onClicked: menu.runAction("cascade")
    }
    PlasmaExtras.MenuItem {
        text: i18n("Show windows stacked")
        icon: "view-split-top-bottom"
        onClicked: menu.runAction("stacked")
    }
    PlasmaExtras.MenuItem {
        text: i18n("Show windows side by side")
        icon: "view-split-left-right"
        onClicked: menu.runAction("side-by-side")
    }
    PlasmaExtras.MenuItem {
        text: i18n("Show the desktop")
        icon: "user-desktop"
        onClicked: menu.runAction("show-desktop")
    }
    PlasmaExtras.MenuItem { separator: true }
    PlasmaExtras.MenuItem {
        text: i18n("Start Task Manager")
        icon: "ksysguardd"
        onClicked: menu.runAction("task-manager")
    }
    PlasmaExtras.MenuItem { separator: true }
    PlasmaExtras.MenuItem {
        text: i18n("Lock the taskbar")
        checkable: true
        checked: menu.taskbarLocked
        onClicked: menu.runAction("toggle-taskbar-lock")
    }
    PlasmaExtras.MenuItem {
        text: i18n("Properties")
        icon: "document-properties"
        onClicked: menu.runAction("taskbar-properties")
    }
}
