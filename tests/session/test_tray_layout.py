#!/usr/bin/env python3
"""Check that layout repair uses the primary tray when panel order changes."""

from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]


class TrayLayoutTest(unittest.TestCase):
    def test_primary_tray_settings_win_even_when_it_is_enumerated_last(self):
        script = (ROOT / "services/session/aero7-shell-layout.js").read_text()
        harness = r"""
const vm = require('vm');
const source = process.argv[1];
const trayType = 'io.gitgud.wackyideas.systemtray';
function tray(extra, shown) {
    return {
        values: {extraItems: extra, shownItems: shown, hiddenItems: [],
                 disabledStatusNotifiers: [], showAllItems: false, itemOrdering: ''},
        readConfig(key, fallback) { return this.values[key] ?? fallback; },
        writeConfig(key, value) { this.values[key] = value; }
    };
}
function panel(screen, trayWidget) {
    const widgets = {[trayType]: [trayWidget]};
    return {
        type: 'io.gitgud.wackyideas.panel', screen,
        widgets(type) { return widgets[type] || []; },
        addWidget(type) { const widget = tray([], []); widgets[type] = [widget]; return widget; },
        remove() { throw Error('unexpected panel removal'); }
    };
}
const primary = panel(0, tray(['io.gitgud.wackyideas.notifications'],
                              ['io.gitgud.wackyideas.notifications']));
const secondary = panel(1, tray(['io.gitgud.wackyideas.battery'], []));
const all = [secondary, primary];
vm.runInNewContext(source, {panels: () => all, screenCount: 2,
                            print: () => {}, Panel: function() { throw Error('new panel'); }});
const left = JSON.stringify(primary.widgets(trayType)[0].values);
const right = JSON.stringify(secondary.widgets(trayType)[0].values);
if (left !== right || right.includes('battery')) process.exit(1);
"""
        result = subprocess.run(["node", "-e", harness, script], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
