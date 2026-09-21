#!/usr/bin/env python3
"""Build and stage both payload profiles without installing onto the host."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InstallProfilesTest(unittest.TestCase):
    def run_command(self, *args, env=None):
        result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, env=env, check=False)
        self.assertEqual(result.returncode, 0, result.stdout[-12000:])

    def test_production_and_opt_in_test_payloads(self):
        with tempfile.TemporaryDirectory(prefix="aero7-install-profiles-") as directory:
            work = Path(directory)
            build = work / "build"
            # Reconfigure the same build to also catch stale install rules when
            # switching back from a test image to a regular developer build.
            for name, build_tests, install_tools in (
                ("production", "OFF", "OFF"),
                ("test-image", "OFF", "ON"),
                ("developer", "ON", "OFF"),
            ):
                with self.subTest(profile=name):
                    self.run_command("cmake", "-S", str(ROOT), "-B", str(build),
                                     "-G", "Ninja", "-DCMAKE_INSTALL_PREFIX=/usr",
                                     f"-DBUILD_TESTING={build_tests}",
                                     f"-DAERO7_INSTALL_TEST_TOOLS={install_tools}")
                    self.run_command("cmake", "--build", str(build), "-j2")
                    stage = work / name
                    self.run_command("cmake", "--install", str(build),
                                     env={**os.environ, "DESTDIR": str(stage)})
                    for path in (
                        "usr/bin/aero7-session", "usr/bin/aero7-recovery-ui",
                        "usr/bin/aero7-recovery", "usr/bin/aero7-migrate",
                        "usr/lib/aero7-desktop/aero7-shell-service",
                        "usr/lib/aero7-desktop/aero7-session-setup",
                        "usr/lib/aero7-desktop/aero7-screenshot-identity",
                        "usr/lib/systemd/user/aero7-shell.service",
                        "usr/share/wayland-sessions/aero7.desktop",
                        "etc/skel/Desktop/Recycle Bin.desktop",
                        "etc/fonts/conf.d/60-aero7-monospace.conf",
                    ):
                        self.assertTrue((stage / path).is_file(), path)
                    expected = install_tools == "ON"
                    for path in (
                        "usr/lib/aero7-desktop/aero7-test-status-notifier",
                        "usr/lib/aero7-desktop/aero7-screenshot-test",
                        "usr/share/applications/org.aero7.visualtest.desktop",
                        "usr/lib/udev/rules.d/99-aero7-ydotool.rules",
                    ):
                        self.assertEqual((stage / path).is_file(), expected, path)
                    scripts = list((stage / "usr/lib/aero7-desktop").glob("test-*.sh"))
                    self.assertEqual(bool(scripts), expected)
                    if expected:
                        self.assertTrue((stage / "usr/lib/aero7-desktop/test-session.sh").is_file())


if __name__ == "__main__":
    unittest.main()
