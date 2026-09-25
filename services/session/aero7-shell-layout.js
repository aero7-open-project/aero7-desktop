// Reconcile the visible Plasma shell to one Aero7 panel on every screen.
// This script is intentionally idempotent: it is run after login and by the
// health service after output changes.

var aeroPanelType = "io.gitgud.wackyideas.panel";
var startType = "io.gitgud.wackyideas.SevenStart";
var tasksType = "io.gitgud.wackyideas.seventasks";
var trayType = "io.gitgud.wackyideas.systemtray";
var clockType = "io.gitgud.wackyideas.digitalclocklite";
var showDesktopType = "io.gitgud.wackyideas.win7showdesktop";
var disabledTrayItems = [
    // Windows 7 does not expose a permanent brightness tray icon. Brightness
    // remains available through hardware keys and Display/Power settings.
    "org.kde.plasma.brightness"
];
var internetExplorerLauncher = "applications:aero7-internet-explorer.desktop";
var defaultLaunchers = [
    "applications:qterminal.desktop",
    "applications:org.aero7.FileExplorer.desktop",
    internetExplorerLauncher
];

function normalizedScreen(panel) {
    // Plasma represents an early-created primary panel as screen -1 until its
    // view is associated. PanelView treats that value as screen 0 as well.
    return panel.screen < 0 ? 0 : panel.screen;
}

function firstWidget(panel, type) {
    var matches = panel.widgets(type);
    return matches.length > 0 ? matches[0] : null;
}

function sameLauncherList(left, right) {
    if (!left || left.length !== right.length) {
        return false;
    }
    for (var index = 0; index < left.length; ++index) {
        if (left[index] !== right[index]) {
            return false;
        }
    }
    return true;
}

function isPreviousFactoryLayout(launchers) {
    var previousLayouts = [
        [
            internetExplorerLauncher,
            "applications:org.aero7.fileexplorer.desktop",
            "applications:linux-controlpanel.desktop",
            "applications:qterminal.desktop"
        ],
        [
            "applications:org.aero7.fileexplorer.desktop",
            "applications:linux-controlpanel.desktop",
            "applications:qterminal.desktop"
        ]
    ];
    for (var index = 0; index < previousLayouts.length; ++index) {
        if (sameLauncherList(launchers, previousLayouts[index])) {
            return true;
        }
    }
    return false;
}

function configureTasks(tasks, launchers) {
    if (!tasks) {
        return;
    }
    tasks.currentConfigGroup = ["General"];
    tasks.writeConfig("showOnlyCurrentScreen", false);
    tasks.writeConfig("showOnlyCurrentDesktop", true);
    tasks.writeConfig("showOnlyCurrentActivity", true);
    tasks.writeConfig("groupingStrategy", 1);
    tasks.writeConfig("groupPopups", true);
    tasks.writeConfig("showPreviews", true);
    tasks.writeConfig("highlightWindows", true);
    tasks.writeConfig("disableJumplists", false);
    tasks.writeConfig("launchers", launchers);
    tasks.writeConfig("internetExplorerPinMigrated", true);
    tasks.writeConfig("windows7FactoryPinLayoutMigrated", true);
}

function withoutDisabledTrayItems(items) {
    if (!items) {
        return [];
    }
    var filtered = [];
    for (var index = 0; index < items.length; ++index) {
        if (disabledTrayItems.indexOf(items[index]) === -1) {
            filtered.push(items[index]);
        }
    }
    return filtered;
}

function configureTray(tray) {
    if (!tray) {
        return;
    }
    tray.currentConfigGroup = ["General"];
    tray.writeConfig("extraItems",
        withoutDisabledTrayItems(tray.readConfig("extraItems", [])));
    tray.writeConfig("shownItems",
        withoutDisabledTrayItems(tray.readConfig("shownItems", [])));
    tray.writeConfig("hiddenItems",
        withoutDisabledTrayItems(tray.readConfig("hiddenItems", [])));
}

function configurePanel(panel, screen, launchers) {
    panel.screen = screen;
    panel.location = "bottom";
    panel.height = 40;
    panel.floating = false;
    panel.hiding = "none";
    panel.lengthMode = "fill";

    var start = firstWidget(panel, startType);
    if (!start) {
        start = panel.addWidget(startType);
    }
    start.globalShortcut = "Meta";

    var tasks = firstWidget(panel, tasksType);
    if (!tasks) {
        tasks = panel.addWidget(tasksType);
    }
    configureTasks(tasks, launchers);

    var tray = firstWidget(panel, trayType);
    if (!tray) {
        tray = panel.addWidget(trayType);
    }
    configureTray(tray);
    if (!firstWidget(panel, clockType)) {
        panel.addWidget(clockType);
    }
    if (!firstWidget(panel, showDesktopType)) {
        panel.addWidget(showDesktopType);
    }
}

var wantedScreens = [];
for (var screen = 0; screen < screenCount; ++screen) {
    wantedScreens.push(screen);
}
if (wantedScreens.length === 0) {
    wantedScreens.push(0);
}

var allPanels = panels();
var keptByScreen = {};
var canonicalLaunchers = null;

// Stock and foreign panels are removed rather than hidden or covered. Keep at
// most one Aero panel for each output.
for (var index = 0; index < allPanels.length; ++index) {
    var candidate = allPanels[index];
    var candidateScreen = normalizedScreen(candidate);
    if (candidate.type !== aeroPanelType || keptByScreen[candidateScreen]) {
        candidate.remove();
        continue;
    }
    keptByScreen[candidateScreen] = candidate;
    var candidateTasks = firstWidget(candidate, tasksType);
    if (candidateTasks) {
        candidateTasks.currentConfigGroup = ["General"];
        var savedLaunchers = candidateTasks.readConfig("launchers", []);
        // Prefer the primary panel even if Plasma enumerates it later. A
        // stale secondary copy must not become the source for every screen.
        if ((!canonicalLaunchers || candidateScreen === 0)
                && savedLaunchers && savedLaunchers.length > 0) {
            canonicalLaunchers = savedLaunchers;
            var factoryPinMigrationDone = candidateTasks.readConfig(
                "windows7FactoryPinLayoutMigrated", false);
            if (!factoryPinMigrationDone && isPreviousFactoryLayout(canonicalLaunchers)) {
                canonicalLaunchers = defaultLaunchers;
            }
        }
    }
}

if (!canonicalLaunchers || canonicalLaunchers.length === 0) {
    canonicalLaunchers = defaultLaunchers;
}

for (var wantedIndex = 0; wantedIndex < wantedScreens.length; ++wantedIndex) {
    var wantedScreen = wantedScreens[wantedIndex];
    var panel = keptByScreen[wantedScreen];
    if (!panel) {
        panel = new Panel(aeroPanelType);
        keptByScreen[wantedScreen] = panel;
    }
    configurePanel(panel, wantedScreen, canonicalLaunchers);
}

print("Aero7 layout reconciled: panels=" + panels().length
    + " screens=" + wantedScreens.length);
