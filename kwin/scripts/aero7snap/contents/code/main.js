function eligible(window) {
    return window && window.managed && window.normalWindow && !window.deleted
        && window.resizeable && !window.fullScreen;
}

function usableArea(window) {
    return workspace.clientArea(KWin.MaximizeArea, window);
}

function setGeometry(window, x, y, width, height) {
    window.setMaximize(false, false);
    const geometry = window.frameGeometry;
    geometry.x = x;
    geometry.y = y;
    geometry.width = width;
    geometry.height = height;
    window.frameGeometry = geometry;
    workspace.activeWindow = window;
}

function snapLeft(window) {
    if (!eligible(window))
        return;
    const area = usableArea(window);
    setGeometry(window, area.x, area.y, Math.floor(area.width / 2), area.height);
}

function snapRight(window) {
    if (!eligible(window))
        return;
    const area = usableArea(window);
    const leftWidth = Math.floor(area.width / 2);
    setGeometry(window, area.x + leftWidth, area.y, area.width - leftWidth, area.height);
}

function attach(window) {
    if (!eligible(window))
        return;
    let edge = 0;
    let edgeEnteredAt = 0;
    const sensitivity = Math.max(0, Math.min(100, Number(readConfig("Sensitivity", 35))));
    const margin = 2 + Math.round(sensitivity / 25);
    const holdMs = 600 - sensitivity * 5;
    window.interactiveMoveResizeStarted.connect(() => { edge = 0; edgeEnteredAt = 0; });
    window.interactiveMoveResizeStepped.connect(geometry => {
        const area = usableArea(window);
        const nextEdge = geometry.y <= area.y + margin ? 3
            : geometry.x <= area.x + margin ? 1
            : geometry.x + geometry.width >= area.x + area.width - margin ? 2 : 0;
        if (nextEdge !== edge) {
            edge = nextEdge;
            edgeEnteredAt = edge ? Date.now() : 0;
        }
        const ready = edge && Date.now() - edgeEnteredAt >= holdMs;
        if (ready && edge === 1)
            workspace.showOutline(area.x, area.y, Math.floor(area.width / 2), area.height);
        else if (ready && edge === 2)
            workspace.showOutline(area.x + Math.floor(area.width / 2), area.y,
                                  Math.ceil(area.width / 2), area.height);
        else if (ready && edge === 3)
            workspace.showOutline(area);
        else
            workspace.hideOutline();
    });
    window.interactiveMoveResizeFinished.connect(() => {
        workspace.hideOutline();
        if (edge && Date.now() - edgeEnteredAt < holdMs) {
            edge = 0;
            return;
        }
        if (edge === 1)
            snapLeft(window);
        else if (edge === 2)
            snapRight(window);
        else if (edge === 3) {
            const area = usableArea(window);
            setGeometry(window, area.x, area.y, area.width, area.height);
        }
        edge = 0;
    });
}

workspace.stackingOrder.forEach(attach);
workspace.windowAdded.connect(attach);
registerShortcut("Aero7 Snap Left Active Window", "Snap the active window to the left half",
                 "Meta+Ctrl+Shift+F11", () => snapLeft(workspace.activeWindow));
