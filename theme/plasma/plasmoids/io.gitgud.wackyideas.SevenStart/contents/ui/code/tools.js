/*
 *    SPDX-FileCopyrightText: 2013 Aurélien Gâteau <agateau@kde.org>
 *    SPDX-FileCopyrightText: 2013-2015 Eike Hein <hein@kde.org>
 *    SPDX-FileCopyrightText: 2017 Ivan Cukic <ivan.cukic@kde.org>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

.pragma library

function fillActionMenu(i18n, actionMenu, actionList, favoriteModel, favoriteId) {
    // Accessing actionList can be a costly operation, so we don't
    // access it until we need the menu.

    var actions = createFavoriteActions(i18n, favoriteModel, favoriteId);

    if (actions) {
        if (actionList && actionList.length > 0) {
            var actionListCopy = Array.from(actionList);
            var separator = { "type": "separator" };
            actionListCopy.push(separator);
            // actionList = actions.concat(actionList); // this crashes Qt O.o
            actionListCopy.push.apply(actionListCopy, actions);
            actionList = actionListCopy;
        } else {
            actionList = actions;
        }
    }

    actionMenu.actionList = actionList;
}

function aeroActionList(i18n, sourceActions, openAction) {
    var blocked = [
        "add widgets", "manage widgets", "edit mode", "enter edit mode",
        "configure panel", "configure taskbar", "configure task manager",
        "show alternatives", "edit application", "kde menu editor",
        "add to desktop", "add to panel", "add to task manager",
        "remove from desktop", "remove from panel",
        "remove panel", "remove this panel"
    ];
    var result = [{
        text: i18n("Open"),
        icon: "document-open",
        action: openAction
    }];
    (sourceActions || []).forEach(function (item) {
        var text = String(item.text || "").replace(/&/g, "").trim().toLowerCase();
        for (var index = 0; index < blocked.length; ++index) {
            if (text.indexOf(blocked[index]) !== -1)
                return;
        }
        if (item.subActions)
            item.subActions = aeroActionList(i18n, item.subActions, openAction).slice(1);
        result.push(item);
    });
    return result;
}

function createFavoriteActions(i18n, favoriteModel, favoriteId) {
    if (!favoriteModel || !favoriteModel.enabled || !favoriteId) {
        return null;
    }

    var action = {};
    if (favoriteModel.isFavorite(favoriteId)) {
        action.text = i18n("Unpin from Start Menu");
        action.icon = "list-remove";
        action.actionId = "_kicker_favorite_remove";
    } else if (favoriteModel.maxFavorites === -1 || favoriteModel.count < favoriteModel.maxFavorites) {
        action.text = i18n("Pin to Start Menu");
        action.icon = "bookmark-new";
        action.actionId = "_kicker_favorite_add";
    } else {
        return null;
    }
    action.actionArgument = { favoriteModel: favoriteModel, favoriteId: favoriteId };
    return [action];
}

function triggerAction(model, index, actionId, actionArgument) {
    function startsWith(txt, needle) {
        return txt.substr(0, needle.length) === needle;
    }

    if (startsWith(actionId, "_kicker_favorite_")) {
        handleFavoriteAction(actionId, actionArgument);
        return;
    }

    var closeRequested = model.trigger(index, actionId, actionArgument);

    if (closeRequested) {
        return true;
    }

    return false;
}

function handleFavoriteAction(actionId, actionArgument) {
    var favoriteId = actionArgument.favoriteId;
    var favoriteModel = actionArgument.favoriteModel;

    if (favoriteModel === null || favoriteId === null) {
        return null;
    }

    if (actionId === "_kicker_favorite_remove") {
        favoriteModel.removeFavorite(favoriteId);
    } else if (actionId === "_kicker_favorite_add") {
        favoriteModel.addFavorite(favoriteId);
    } else if (actionId === "_kicker_favorite_remove_from_activity") {
        favoriteModel.removeFavoriteFrom(favoriteId, actionArgument.favoriteActivity);
    } else if (actionId === "_kicker_favorite_add_to_activity") {
        favoriteModel.addFavoriteTo(favoriteId, actionArgument.favoriteActivity);
    } else if (actionId === "_kicker_favorite_set_to_activity") {
        favoriteModel.setFavoriteOn(favoriteId, actionArgument.favoriteActivity);
    }
}
