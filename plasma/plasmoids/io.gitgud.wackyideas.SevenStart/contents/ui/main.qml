/***************************************************************************
 *   Copyright (C) 2014-2015 by Eike Hein <hein@kde.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/
pragma ComponentBehavior: Bound
import QtQuick 2.0
import QtCore
import QtQuick.Layouts 1.1
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.private.kicker 0.1 as Kicker
import org.kde.kquickcontrolsaddons as KQuickControlsAddons
import org.kde.kwindowsystem
import org.kde.ksvg as KSvg
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasma5support as Plasma5Support

PlasmoidItem {
    id: kicker

    signal reset

    property Item dragSource: null
    // Plasma 6.7 can finish constructing the Kicker delegate models before
    // this applet has been attached to its panel containment.  Giving Kicker
    // the interface during that window lets its launcher-action code
    // dereference a null Plasma::Applet containment and crashes plasmashell.
    // Delay model activation until the next event-loop turn, when the applet
    // and panel relationship is complete.
    property bool containmentReady: false

    property alias rootModel: rootModel
    property QtObject globalFavorites: rootModel.favoritesModel
    property QtObject systemFavorites: rootModel.systemFavoritesModel
    property bool compositingEnabled: KWindowSystem.isPlatformX11 ? KX11Extras.compositingActive : true

    // Keep development and service helpers out of the browsable Windows-style
    // program list. They remain available when their complete display name is
    // entered in search so administrators can still reach them deliberately.
    readonly property var exactSearchOnlyApplicationNames: [
        "Avahi SSH Server Browser",
        "Avahi VNC Server Browser",
        "Avahi Zeroconf Browser",
        "Hardware Locality lstopo",
        "Kvantum Manager",
        "Manage Printing",
        "Menu Editor",
        "Qt Assistant",
        "Qt D-Bus Viewer",
        "Qt Linguist",
        "Qt V4L2 test Utility",
        "Qt V4L2 video capture utility",
        "Qt Widgets Designer"
    ]
    readonly property var fullyHiddenApplicationNames: [
        "Emoji Selector",
        "System Settings"
    ]

    function normalizedApplicationName(value) {
        return String(value || "").trim().toLocaleLowerCase();
    }

    function nameIsInList(name, values) {
        const normalizedName = normalizedApplicationName(name);
        for (let i = 0; i < values.length; ++i) {
            if (normalizedApplicationName(values[i]) === normalizedName) {
                return true;
            }
        }
        return false;
    }

    function isExactSearchOnlyApplication(name) {
        return nameIsInList(name, exactSearchOnlyApplicationNames);
    }

    function isFullyHiddenApplication(name) {
        return nameIsInList(name, fullyHiddenApplicationNames);
    }

    function shouldShowApplicationInSearch(name, query) {
        if (isFullyHiddenApplication(name)) {
            return false;
        }
        if (!isExactSearchOnlyApplication(name)) {
            return true;
        }
        return normalizedApplicationName(name) === normalizedApplicationName(query);
    }

    function sanitizedFavoriteApplications(values) {
        const cleaned = [
            "applications:org.aero7.fileexplorer.desktop",
            "applications:linux-controlpanel.desktop"
        ];
        for (let index = 0; values && index < values.length; ++index) {
            let favorite = String(values[index] || "");
            const normalized = favorite.toLocaleLowerCase();
            if (normalized.indexOf("systemsettings") !== -1) {
                favorite = "applications:linux-controlpanel.desktop";
            } else if (normalized.indexOf("dolphin") !== -1
                       || normalized.indexOf("fileexplorer") !== -1) {
                favorite = "applications:org.aero7.fileexplorer.desktop";
            }
            if (favorite && cleaned.indexOf(favorite) === -1) {
                cleaned.push(favorite);
            }
        }
        return cleaned;
    }

    function sanitizeFavoriteApplications() {
        if (!globalFavorites || !globalFavorites.favorites) {
            return;
        }
        const current = Array.from(globalFavorites.favorites);
        const cleaned = sanitizedFavoriteApplications(current);
        if (JSON.stringify(current) !== JSON.stringify(cleaned)) {
            globalFavorites.favorites = cleaned;
        }
    }

    Plasmoid.constraintHints: Plasmoid.CanFillArea
    activationTogglesExpanded: false

    toolTipMainText: i18n("Start")
    toolTipSubText: ""

    CompactRepresentation { id: compactRepresentation; anchors.fill: parent }

    // Used to run separate programs through this plasmoid.
    Plasma5Support.DataSource {
    	id: menu_executable
    	engine: "executable"
    	connectedSources: []
    	onNewData: (sourceName, data) => {
    	    var exitCode = data["exit code"]
    	    var exitStatus = data["exit status"]
    	    var stdout = data["stdout"]
    	    var stderr = data["stderr"]
    	    exited(sourceName, exitCode, exitStatus, stdout, stderr)
    	    disconnectSource(sourceName)
    	}
    	function exec(cmd) {
    	    if (cmd) {
    	        connectSource(cmd)
    	    }
    	}
    	signal exited(string cmd, int exitCode, int exitStatus, string stdout, string stderr)
    }

    SidePanelModels {
        id: sidePanelModels
    }
    Kicker.WindowSystem {
        id: windowSystem
    }

    Kicker.RecentUsageModel {
        id: recentUsageModel
        favoritesModel: globalFavorites
        ordering: 0
        shownItems: Kicker.RecentUsageModel.OnlyApps
    }
    Kicker.RunnerModel {
        id: runnerModel

        appletInterface: kicker.containmentReady ? kicker : null
        favoritesModel: globalFavorites
        mergeResults: true
    }
    Kicker.RootModel {
        id: rootModel

        autoPopulate: false

        appNameFormat: Plasmoid.configuration.appNameFormat
        flat: true
        sorted: true
        showSeparators: false
        appletInterface: kicker.containmentReady ? kicker : null

        paginate: false
        pageSize: Plasmoid.configuration.numberColumns *  Plasmoid.configuration.numberRows

        showTopLevelItems: true
        showAllApps: true
        showAllAppsCategorized: false
        showRecentApps: false
        showRecentDocs: false
        highlightNewlyInstalledApps: true
        showPowerSession: false

        onFavoritesModelChanged: {
            if ("initForClient" in favoritesModel) {
                favoritesModel.initForClient("org.kde.plasma.kicker.favorites.instance-" + Plasmoid.id)

                if (!Plasmoid.configuration.favoritesPortedToKAstats) {
                    favoritesModel.portOldFavorites(Plasmoid.configuration.favoriteApps);
                    Plasmoid.configuration.favoritesPortedToKAstats = true;
                }
            } else {
                favoritesModel.favorites = Plasmoid.configuration.favoriteApps;
            }
        }

        onSystemFavoritesModelChanged: {
            systemFavoritesModel.enabled = false;
            systemFavoritesModel.favorites = Plasmoid.configuration.favoriteSystemActions;
            systemFavoritesModel.maxFavorites = 8;
        }

        Component.onCompleted: {
            if ("initForClient" in favoritesModel) {
                favoritesModel.initForClient("org.kde.plasma.kicker.favorites.instance-" + Plasmoid.id)

                if (!Plasmoid.configuration.favoritesPortedToKAstats) {
                    favoritesModel.portOldFavorites(Plasmoid.configuration.favoriteApps);
                    Plasmoid.configuration.favoritesPortedToKAstats = true;
                }
            } else {
                favoritesModel.favorites = Plasmoid.configuration.favoriteApps;
            }
            Qt.callLater(kicker.sanitizeFavoriteApplications);
        }
    }

    Connections {
        target: globalFavorites

        function onFavoritesChanged() {
            const current = Array.from(target.favorites || []);
            const cleaned = kicker.sanitizedFavoriteApplications(current);
            if (JSON.stringify(current) !== JSON.stringify(cleaned)) {
                target.favorites = cleaned;
                return;
            }
            Plasmoid.configuration.favoriteApps = cleaned;
        }
    }

    Connections {
        target: systemFavorites

        function onFavoritesChanged() {
            Plasmoid.configuration.favoriteSystemActions = target.favorites;
        }
    }

    Connections {
        target: Plasmoid.configuration

        function onFavoriteAppsChanged() {
            globalFavorites.favorites = Plasmoid.configuration.favoriteApps;
        }

        function onFavoriteSystemActionsChanged() {
            systemFavorites.favorites = Plasmoid.configuration.favoriteSystemActions;
        }
    }

    Kicker.DragHelper {
        id: dragHelper
    }

    Kicker.ProcessRunner {
        id: processRunner
    }

	// SVGs
    KSvg.FrameSvgItem {
        id : highlightItemSvg
        visible: false
        imagePath: Qt.resolvedUrl("svgs/menuitem.svg")
        prefix: "hover"
    }
    KSvg.FrameSvgItem {
        id : panelSvg
        visible: false
        imagePath: "widgets/panel-background"
    }
    KSvg.Svg {
        id: arrowsSvg
        imagePath: Qt.resolvedUrl("svgs/arrows.svgz")
        size: "16x16"
    }
    KSvg.Svg {
        id: separatorSvg
        imagePath: Qt.resolvedUrl("svgs/sidebarseparator.svg")
    }
    KSvg.Svg {
        id: lockScreenSvg
        imagePath: Qt.resolvedUrl("svgs/system-lock-screen.svg")
    }

    PlasmaComponents.Label {
        id: toolTipDelegate

        width: contentWidth
        height: contentHeight

        property Item toolTip

        text: (toolTip != null) ? toolTip.text : ""
    }

    function isValidUrl(url) {
        if(url) {
            if(url.toString().startsWith("file:///usr/share/applications") ||
               url.toString().startsWith("file:///usr/local/share/applications") ||
               url.toString().startsWith(StandardPaths.writableLocation(StandardPaths.ApplicationsLocation)))
                return true;
        }
        return false;
    }
    function convertUrl(url) {
        return "applications:" + url.toString().split("/").pop();
    }
    function resetDragSource() {
        dragSource = null;
    }
    function enableHideOnWindowDeactivate() {
        kicker.hideOnWindowDeactivate = true;
    }

    function runShellAction(action) {
        menu_executable.exec("aero7-shell-action " + action);
    }

    // The orb has an Aero7-owned two-item context menu. Publishing applet
    // contextual actions here lets the containment append Plasma's Configure,
    // Alternatives and edit-mode actions, so expose none.
    Plasmoid.contextualActions: []

    Component.onCompleted: {
        windowSystem.focusIn.connect(enableHideOnWindowDeactivate);
        kicker.hideOnWindowDeactivate = true;

        dragHelper.dropped.connect(resetDragSource);
        containmentReadyTimer.start();
    }

    Timer {
        id: containmentReadyTimer
        interval: 250
        repeat: false
        onTriggered: kicker.containmentReady = true
    }

}
