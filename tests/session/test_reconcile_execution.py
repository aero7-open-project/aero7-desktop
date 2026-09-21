#!/usr/bin/env python3
"""Execute the setup helper against isolated command doubles, never the host DE."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ReconcileExecutionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        commands = self.root / "bin"
        commands.mkdir()
        self.log = self.root / "commands.jsonl"
        stub = commands / "stub"
        stub.write_text(f"#!{sys.executable}\n" + r'''
import json, os, sys
from pathlib import Path
name = Path(sys.argv[0]).name
with open(os.environ['A7_TEST_COMMAND_LOG'], 'a') as stream:
    stream.write(json.dumps([name, *sys.argv[1:]]) + '\n')
if name == 'identity':
    sys.exit(1)
if name == 'qdbus6' and sys.argv[-1] == 'org.kde.PlasmaShell.shell':
    print('io.gitgud.wackyideas.desktop')
if name == 'kreadconfig6' and sys.argv[-1] == 'ColorScheme':
    print(os.environ['A7_TEST_SCHEME'])
if name == 'xdg-user-dir':
    print(os.environ['HOME'] + '/Desktop')
''', encoding="utf-8")
        stub.chmod(0o755)
        for name in ("identity", "qdbus6", "kreadconfig6", "kwriteconfig6",
                     "kvantummanager", "systemctl", "plasma-apply-wallpaperimage",
                     "kbuildsycoca6", "xdg-user-dir"):
            (commands / name).symlink_to(stub)
        layout = self.root / "layout.js"
        layout.write_text("// isolated layout fixture\n")
        wallpaper = self.root / "wallpaper.png"
        wallpaper.touch()
        source = (ROOT / "services/session/aero7-session-setup").read_text()
        # Only absolute resources are redirected; control flow is the real script.
        replacements = {
            "/usr/lib/aero7-desktop/aero7-screenshot-identity": str(commands / "identity"),
            "/usr/share/aero7-desktop/shell/aero7-shell-layout.js": str(layout),
            "/usr/share/wallpapers/Aero7/contents/images/1672x941.png": str(wallpaper),
            "/etc/skel/Desktop/Recycle Bin.desktop": str(self.root / "unused-template"),
        }
        for original, isolated in replacements.items():
            self.assertIn(original, source)
            source = source.replace(original, isolated)
        self.script = self.root / "setup"
        self.script.write_text(source)
        self.env = dict(os.environ, HOME=str(self.root / "home"),
                        XDG_CONFIG_HOME=str(self.root / "config"),
                        XDG_STATE_HOME=str(self.root / "state"),
                        PATH=str(commands) + os.pathsep + os.environ["PATH"],
                        A7_TEST_COMMAND_LOG=str(self.log), A7_TEST_SCHEME="Aero7Light")

    def run_setup(self, *args, scheme="Aero7Light"):
        self.env["A7_TEST_SCHEME"] = scheme
        result = subprocess.run(["bash", str(self.script), *args], env=self.env,
                                text=True, capture_output=True, timeout=15)
        calls = [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []
        return result, calls

    def test_layout_repair_is_live_and_does_not_rewrite_appearance(self):
        result, calls = self.run_setup("--reconcile-layout-only")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(any("org.kde.PlasmaShell.evaluateScript" in call for call in calls))
        self.assertTrue(any(call[0] == "plasma-apply-wallpaperimage" for call in calls))
        self.assertFalse(any(call[:3] == ["systemctl", "--user", "restart"] for call in calls))
        self.assertFalse(any(call[0] == "kvantummanager" for call in calls))
        self.assertFalse(any(call[0] == "kwriteconfig6" and "kdeglobals" in call for call in calls))

    def test_appearance_repair_restarts_and_reasserts_layout(self):
        result, calls = self.run_setup("--reconcile-only", scheme="BreezeDark")
        self.assertEqual(result.returncode, 0, result.stderr)
        restart = ["systemctl", "--user", "restart", "plasma-plasmashell.service"]
        self.assertEqual(calls.count(restart), 1)
        self.assertIn(["kvantummanager", "--set", "KvDark"], calls)
        after = calls[calls.index(restart) + 1:]
        self.assertTrue(any("org.kde.PlasmaShell.evaluateScript" in call for call in after))
        self.assertTrue(any(call[0] == "plasma-apply-wallpaperimage" for call in after))

    def test_initial_login_never_restarts_shell(self):
        result, calls = self.run_setup()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(["kvantummanager", "--set", "Windows7Aero"], calls)
        self.assertFalse(any(call[:3] == ["systemctl", "--user", "restart"] for call in calls))

    def test_extra_arguments_are_rejected_before_any_action(self):
        result, calls = self.run_setup("--reconcile-only", "unexpected")
        self.assertEqual(result.returncode, 64)
        self.assertEqual(calls, [])


if __name__ == "__main__":
    unittest.main()
