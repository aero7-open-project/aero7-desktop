let restoreWindows = [];
const trackers = new Map();

function onCurrentDesktop(window) {
    return window.onAllDesktops || window.desktops.indexOf(workspace.currentDesktop) !== -1;
}

function onCurrentActivity(window) {
    return window.activities.length === 0
        || window.activities.indexOf(workspace.currentActivity) !== -1;
}

function eligible(window) {
    return window && window.managed && window.normalWindow && !window.deleted
        && !window.skipTaskbar && !window.fullScreen && window.minimizable
        && onCurrentDesktop(window) && onCurrentActivity(window);
}

function shake(window) {
    if (!eligible(window))
        return;
    if (restoreWindows.length > 0) {
        const pending = restoreWindows;
        restoreWindows = [];
        pending.forEach(other => {
            if (other && !other.deleted && other.managed && other.minimized)
                other.minimized = false;
        });
        workspace.activeWindow = window;
        return;
    }
    const minimized = [];
    workspace.stackingOrder.forEach(other => {
        if (other !== window && eligible(other) && !other.minimized) {
            other.minimized = true;
            minimized.push(other);
        }
    });
    restoreWindows = minimized;
    workspace.activeWindow = window;
}

function attach(window) {
    if (!eligible(window))
        return;
    const key = window.internalId.toString();
    const state = {tracking: false, triggered: false, anchor: 0, direction: 0,
                   reversals: 0, minimum: 0, maximum: 0, startedAt: 0};
    trackers.set(key, state);
    window.interactiveMoveResizeStarted.connect(() => {
        const center = window.frameGeometry.x + window.frameGeometry.width / 2;
        Object.assign(state, {tracking: true, triggered: false, anchor: center,
                              direction: 0, reversals: 0, minimum: center,
                              maximum: center, startedAt: Date.now()});
    });
    window.interactiveMoveResizeStepped.connect(geometry => {
        if (!state.tracking || state.triggered)
            return;
        if (Date.now() - state.startedAt > 1000) {
            state.tracking = false;
            return;
        }
        const center = geometry.x + geometry.width / 2;
        state.minimum = Math.min(state.minimum, center);
        state.maximum = Math.max(state.maximum, center);
        const delta = center - state.anchor;
        const direction = delta > 0 ? 1 : delta < 0 ? -1 : 0;
        if (direction === 0 || Math.abs(delta) < 22)
            return;
        if (state.direction !== 0 && direction !== state.direction)
            state.reversals++;
        state.direction = direction;
        state.anchor = center;
        if (state.reversals >= 4 && state.maximum - state.minimum >= 90) {
            state.triggered = true;
            shake(window);
        }
    });
    window.interactiveMoveResizeFinished.connect(() => { state.tracking = false; });
    window.closed.connect(() => {
        trackers.delete(key);
        restoreWindows = restoreWindows.filter(candidate => candidate !== window);
    });
}

workspace.stackingOrder.forEach(attach);
workspace.windowAdded.connect(attach);
registerShortcut("Aero7 Shake Active Window", "Exercise Aero7 Shake on the active window",
                 "Meta+Ctrl+Shift+F12", () => shake(workspace.activeWindow));
