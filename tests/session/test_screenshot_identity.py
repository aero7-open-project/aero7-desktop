import runpy
from pathlib import Path
import tempfile
import unittest

MODULE = runpy.run_path(str(Path(__file__).resolve().parents[2] /
                           "services/session/aero7-screenshot-identity"))
repair = MODULE["reconcile"]
read = MODULE["read_entry"]


class ScreenshotIdentityTest(unittest.TestCase):
    def test_legacy_override_and_hidden_entry_keep_upstream_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            upstream = root / "spectacle.desktop"
            upstream.write_text(
                "[Desktop Entry]\nName=Spectacle\nName[nl]=Spectacle\n"
                "Exec=/usr/bin/spectacle\nX-KDE-Shortcuts=Print,Meta+Shift+S\n"
                "X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2\n"
                "X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1\n")
            applications, state = root / "apps", root / "state"
            applications.mkdir()
            target = applications / "org.kde.spectacle.desktop"
            legacy = "[Desktop Entry]\nName=Snipping Tool\nExec=/usr/bin/spectacle -r -b -c\nHidden=true\n"
            target.write_text(legacy)
            print_target = applications / "aero7-snipping-tool-print.desktop"
            print_target.write_text("[Desktop Entry]\nExec=/usr/bin/spectacle -r -b -c\nX-KDE-Shortcuts=Print\nNoDisplay=true\n")
            self.assertTrue(repair(upstream, applications, state))
            entry = read(target)["Desktop Entry"]
            self.assertEqual(entry["X-KDE-DBUS-Restricted-Interfaces"], "org.kde.KWin.ScreenShot2")
            self.assertEqual(entry["X-KDE-Wayland-Interfaces"], "zkde_screencast_unstable_v1")
            self.assertEqual(entry["Exec"], "/usr/bin/spectacle")
            self.assertEqual(entry["NoDisplay"], "true")
            self.assertNotIn("Hidden", entry)
            self.assertNotIn("X-KDE-Shortcuts", entry)
            self.assertEqual(read(print_target)["Desktop Entry"]["Exec"],
                             "/usr/bin/aero7-snipping-tool --capture")
            self.assertEqual(read(print_target)["Desktop Entry"]["X-KDE-Shortcuts"], "Print")
            self.assertEqual((state / "spectacle-before-identity.desktop").read_text(), legacy)
            self.assertFalse(repair(upstream, applications, state))
            target.write_text("[Desktop Entry]\nExec=my-custom-wrapper\n")
            with self.assertRaisesRegex(RuntimeError, "Custom"):
                repair(upstream, applications, state)
            upstream.write_text("[Desktop Entry]\nExec=/usr/bin/spectacle\n")
            with self.assertRaisesRegex(RuntimeError, "authorization"):
                repair(upstream, applications, state)


if __name__ == "__main__":
    unittest.main()
