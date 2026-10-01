/*
    Copyright (C) 2011  Martin Gräßlin <mgraesslin@kde.org>
    Copyright (C) 2012  Gregor Taetzner <gregor@freenet.de>
    Copyright (C) 2015-2018  Eike Hein <hein@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
import QtQuick 2.0
import QtQuick.Layouts 1.15
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.plasma5support as Plasma5Support

import org.kde.plasma.private.kicker as Kicker
import org.kde.kitemmodels as KItemModels

import org.kde.kirigami as Kirigami

Item {
    id: searchViewContainer

    property Item itemGrid: resultsGrid
    property bool queryFinished: false
    property int repeaterModelIndex: 0
    property var unfilteredRunnerModel: null
    property var controlPanelCatalog: []
    property var computerManagementCatalog: []
    property var deviceManagerCatalog: []
    property int runnerResultCount: 0

    // Keep local Aero7 settings responsive while coalescing expensive
    // KRunner queries during a burst of typing.
    Timer {
        id: queryDelay
        interval: 120
        repeat: false
        onTriggered: runnerModel.query = searchField.text
    }

    function normalized(value) {
        return String(value || "").trim().toLocaleLowerCase();
    }

    function appendCatalog(catalog, query, provider, sectionLabel) {
        for (let i = 0; i < catalog.length; ++i) {
            const setting = catalog[i];
            const haystack = normalized(setting.name + " " + setting.description
                + " " + setting.section + " " + setting.keywords);
            if (haystack.indexOf(query) === -1) {
                continue;
            }
            combinedResultsModel.append({
                "key": setting.key,
                "provider": provider,
                "sourceRow": -1,
                "name": setting.name,
                "display": setting.name,
                "decoration": setting.icon || "preferences-system",
                "description": setting.description,
                "group": sectionLabel,
                "favoriteId": "",
                "hasActionList": false,
                "actionList": [],
                "url": ""
            });
        }
    }

    function modelData(model, row, role) {
        return model.data(model.index(row, 0), role);
    }

    function appendRunnerResults(applicationsOnly) {
        if (!unfilteredRunnerModel) {
            return;
        }
        const resultCount = filteredRunnerModel.rowCount();
        for (let row = 0; row < resultCount; ++row) {
            const groupName = String(modelData(filteredRunnerModel, row, 258)
                || i18n("Programs"));
            const isApplication = groupName === i18n("Applications")
                || groupName === i18n("Programs");
            if (isApplication !== applicationsOnly) {
                continue;
            }
            const displayName = modelData(filteredRunnerModel, row, Qt.DisplayRole);
            combinedResultsModel.append({
                "key": "",
                "provider": "runner",
                "sourceRow": row,
                "name": String(displayName || ""),
                "display": String(displayName || ""),
                "decoration": modelData(filteredRunnerModel, row, Qt.DecorationRole),
                "description": String(modelData(filteredRunnerModel, row, 257) || ""),
                "group": groupName,
                "favoriteId": String(modelData(filteredRunnerModel, row, 259) || ""),
                "hasActionList": Boolean(modelData(filteredRunnerModel, row, 264)),
                "actionList": modelData(filteredRunnerModel, row, 265) || [],
                "url": String(modelData(filteredRunnerModel, row, 266) || "")
            });
        }
        runnerResultCount = resultCount;
    }

    function rebuildCombinedResults() {
        combinedResultsModel.clear();
        runnerResultCount = 0;
        const query = normalized(searchField.text);
        if (!query) {
            return;
        }

        // One unified list: applications first, Aero7 Control Panel and
        // Device Manager sections next, then files and other runners.
        // Computer Management pages are normal application desktop entries,
        // so they naturally appear in the first group without duplicates.
        appendRunnerResults(true);
        appendCatalog(controlPanelCatalog, query, "control-panel", i18n("Control Panel"));
        appendCatalog(deviceManagerCatalog, query, "device-manager", i18n("Device Manager"));
        appendRunnerResults(false);

    }

    ListModel {
        id: combinedResultsModel
        dynamicRoles: true
    }

    QtObject {
        id: combinedActionModel
        property var favoritesModel: filteredRunnerModel.favoritesModel

        function trigger(index, actionId, actionArgument) {
            if (index < 0 || index >= combinedResultsModel.count) {
                return false;
            }
            const result = combinedResultsModel.get(index);
            if (result.provider === "runner") {
                return filteredRunnerModel.trigger(result.sourceRow, actionId, actionArgument);
            }
            if (result.provider === "computer-management") {
                settingsLauncher.exec("/usr/bin/aero7-compmgmt --open " + result.key);
            } else if (result.provider === "device-manager") {
                settingsLauncher.exec("/usr/bin/devmgmt --open " + result.key);
            } else {
                settingsLauncher.exec("/usr/bin/control --setting " + result.key);
            }
            return true;
        }
    }

    Plasma5Support.DataSource {
        id: settingsCatalogLoader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            if (data["exit code"] !== 0) {
                return;
            }
            try {
                controlPanelCatalog = JSON.parse(data["stdout"] || "[]");
            } catch (error) {
                console.warn("SevenStart could not read Control Panel settings:", error);
                controlPanelCatalog = [];
            }
            rebuildCombinedResults();
        }
        Component.onCompleted: connectSource("/usr/bin/control --list-settings-json")
    }

    Plasma5Support.DataSource {
        id: computerManagementCatalogLoader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            if (data["exit code"] !== 0) {
                return;
            }
            try {
                computerManagementCatalog = JSON.parse(data["stdout"] || "[]");
            } catch (error) {
                console.warn("SevenStart could not read Computer Management settings:", error);
                computerManagementCatalog = [];
            }
            rebuildCombinedResults();
        }
        Component.onCompleted: connectSource("/usr/bin/aero7-compmgmt --list-settings-json")
    }

    Plasma5Support.DataSource {
        id: deviceManagerCatalogLoader
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => {
            disconnectSource(sourceName);
            if (data["exit code"] !== 0) {
                return;
            }
            try {
                deviceManagerCatalog = JSON.parse(data["stdout"] || "[]");
            } catch (error) {
                console.warn("SevenStart could not read Device Manager sections:", error);
                deviceManagerCatalog = [];
            }
            rebuildCombinedResults();
        }
        Component.onCompleted: connectSource("/usr/bin/devmgmt --list-settings-json")
    }

    Plasma5Support.DataSource {
        id: settingsLauncher
        engine: "executable"
        connectedSources: []
        onNewData: (sourceName, data) => disconnectSource(sourceName)
        function exec(command) {
            if (command) {
                connectSource(command);
            }
        }
    }

    KItemModels.KSortFilterProxyModel {
        id: filteredRunnerModel

        sourceModel: searchViewContainer.unfilteredRunnerModel
        readonly property var favoritesModel: sourceModel ? sourceModel.favoritesModel : null

        function trigger(proxyRow, actionId, actionArgument) {
            if (!sourceModel) {
                return false;
            }
            const sourceIndex = mapToSource(index(proxyRow, 0));
            if (sourceIndex.row < 0) {
                return false;
            }
            return sourceModel.trigger(sourceIndex.row, actionId || "", actionArgument ?? null);
        }

        filterRowCallback: function(sourceRow, sourceParent) {
            if (!sourceModel) {
                return false;
            }
            const sourceIndex = sourceModel.index(sourceRow, 0, sourceParent);
            const displayName = sourceModel.data(sourceIndex, Qt.DisplayRole);
            return kicker.shouldShowApplicationInSearch(displayName, searchField.text);
        }
    }

    function refreshResultsModel() {
        unfilteredRunnerModel = runnerModel.count ? runnerModel.modelForRow(0) : null;
        filteredRunnerModel.invalidateFilter();
        rebuildCombinedResults();
    }

    function inhibitMouse() {
        itemGrid.inhibitMouseEvents = 2;
    }
    function activateCurrentIndex() {
        itemGrid.tryActivate();
    }
    function decrementCurrentIndex() {
        inhibitMouse();
        var listView = itemGrid.flickableItem;
        if(listView.currentIndex-1 < 0) {
            listView.currentIndex = listView.count - 1;
        } else {
            listView.currentIndex--;
        }
    }
    function incrementCurrentIndex() {
        inhibitMouse();
        var listView = itemGrid.flickableItem;
        if(listView.currentIndex+1 >= listView.count) {
            listView.currentIndex = 0;
        } else {
            listView.currentIndex++;
        }
    }
    function onQueryChanged() {
        queryFinished = false;
        queryDelay.stop();
        unfilteredRunnerModel = null;
        rebuildCombinedResults();
        if (!searchField.text) {
            runnerModel.query = "";
        } else {
            queryDelay.start();
        }
    }
    function openContextMenu() {
        runnerModel.currentItem.openActionMenu();
    }

    objectName: "SearchView"

    Connections {
        function onCountChanged() {
            if (runnerModel.query === searchField.text
                    && runnerModel.count && !unfilteredRunnerModel) {
                refreshResultsModel();
            }
        }
        function onQueryFinished() {
            if (runnerModel.query === searchField.text) {
                refreshResultsModel();
                queryFinished = true;
                var listView = resultsGrid.flickableItem;
                if(listView.count > 0) listView.currentIndex = 0;
                //console.log(runnerModel.modelForRow(0).modelForRow(0))
            }
        }

        target: runnerModel
    }

    NavGrid {
        id: resultsGrid
        anchors.fill: parent
        triggerModel: combinedResultsModel
        actionModel: combinedActionModel

        MouseArea {
            id: mouseInhibitor
            anchors.fill: parent
            z: 99
            hoverEnabled: true
            visible: resultsGrid.inhibitMouseEvents > 0
            onPositionChanged: {
                if(resultsGrid.inhibitMouseEvents > 0) {
                    resultsGrid.inhibitMouseEvents--;
                }
            }
        }
    }
}
