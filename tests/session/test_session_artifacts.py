#!/usr/bin/env python3

import configparser
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SessionArtifactsTest(unittest.TestCase):
    def test_session_files_are_distinct_wayland_sessions(self):
        normal = self.read_desktop("aero7.desktop")
        safe = self.read_desktop("aero7-safe.desktop")
        self.assertEqual(normal["Name"], "Aero7 Desktop")
        self.assertEqual(normal["Exec"], "/usr/bin/aero7-session")
        self.assertIn("--safe-mode", safe["Exec"])
        self.assertEqual(normal["X-KDE-SessionType"], "wayland")
        self.assertEqual(normal["TryExec"], "/usr/bin/aero7-session")

    def test_desktop_file_validator_when_available(self):
        validator = shutil.which("desktop-file-validate")
        if validator is None:
            self.skipTest("desktop-file-utils is not installed")
        files = list((ROOT / "packaging" / "session").glob("*.desktop"))
        files.extend((ROOT / "packaging" / "applications").glob("*.desktop"))
        for path in files:
            subprocess.run(
                (validator, str(path)),
                check=True,
            )

    def test_file_explorer_route_uses_the_aero7_wrapper(self):
        desktop = self.read_desktop_path(
            ROOT / "packaging/applications/org.aero7.fileexplorer.desktop"
        )
        self.assertEqual(desktop["Name"], "File Explorer")
        self.assertEqual(desktop["Exec"], "aero7-file-explorer %U")
        self.assertEqual(desktop["StartupWMClass"], "org.kde.dolphin")
        wrapper = (ROOT / "integration/file-explorer/aero7-file-explorer").read_text(
            encoding="utf-8"
        )
        self.assertIn("/usr/bin/dolphin", wrapper)
        self.assertIn("LANGUAGE", wrapper)

    def test_atpootb_is_limited_to_the_aero_compatibility_shell(self):
        dropin = (
            ROOT
            / "packaging/systemd/10-aero7-atpootb.conf"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "ConditionEnvironment=PLASMA_DEFAULT_SHELL="
            "io.gitgud.wackyideas.desktop",
            dropin,
        )
        installer = (
            ROOT / "packaging/install-systemd-dropins.sh"
        ).read_text(encoding="utf-8")
        self.assertIn(r"app-x\x2datpootb@autostart.service.d", installer)

    @staticmethod
    def read_desktop(name: str):
        return SessionArtifactsTest.read_desktop_path(
            ROOT / "packaging/session" / name
        )

    @staticmethod
    def read_desktop_path(path: Path):
        parser = configparser.ConfigParser(interpolation=None)
        parser.optionxform = str
        parser.read(path, encoding="utf-8")
        return parser["Desktop Entry"]


if __name__ == "__main__":
    unittest.main()
