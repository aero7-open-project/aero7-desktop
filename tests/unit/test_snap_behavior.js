const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const script = fs.readFileSync(path.join(__dirname,
    "../../kwin/scripts/aero7snap/contents/code/main.js"), "utf8");

function drag(sensitivity, duration) {
    let clock = 1000;
    let changed = 0;
    const signals = {};
    for (const name of ["interactiveMoveResizeStarted", "interactiveMoveResizeStepped",
                        "interactiveMoveResizeFinished"])
        signals[name] = {connect: callback => { signals[name].callback = callback; }};
    const window = {
        managed: true, normalWindow: true, deleted: false, resizeable: true,
        fullScreen: false, frameGeometry: {x: 100, y: 100, width: 300, height: 300},
        setMaximize: () => { changed++; }, ...signals,
    };
    const workspace = {
        stackingOrder: [window], windowAdded: {connect: () => {}},
        clientArea: () => ({x: 0, y: 0, width: 1000, height: 700}),
        showOutline: () => {}, hideOutline: () => {}, activeWindow: window,
    };
    vm.runInNewContext(script, {
        workspace, KWin: {MaximizeArea: 0},
        Date: {now: () => clock},
        readConfig: () => sensitivity,
        registerShortcut: () => {},
    });
    signals.interactiveMoveResizeStarted.callback();
    signals.interactiveMoveResizeStepped.callback({x: 100, y: 0, width: 300, height: 300});
    clock += duration;
    signals.interactiveMoveResizeFinished.callback();
    return changed;
}

assert.equal(drag(35, 50), 0, "a quick edge brush must not snap");
assert.equal(drag(35, 450), 1, "holding at the edge should snap");
assert.equal(drag(0, 450), 0, "low sensitivity should require a longer hold");
assert.equal(drag(100, 150), 1, "high sensitivity should snap sooner");
