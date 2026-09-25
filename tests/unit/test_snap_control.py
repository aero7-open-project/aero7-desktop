#!/usr/bin/env python3
"""Exercise the same per-user switch used by Aero7 Features and Control Panel."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PROGRAM = Path(__file__).resolve().parents[2] / "services/session/aero7-snap-control"


class SnapControlTest(unittest.TestCase):
    def test_switch_and_sensitivity_persist(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "kwinrc"
            env = dict(os.environ, AERO7_SNAP_KWINRC=str(config),
                       AERO7_SNAP_NO_RECONFIGURE="1")

            def run(*arguments, check=True):
                return subprocess.run([sys.executable, str(PROGRAM), *arguments],
                                      env=env, capture_output=True, text=True, check=check)

            self.assertEqual(run("status").stdout.strip(), "enabled")
            self.assertEqual(run("sensitivity").stdout.strip(), "35")
            run("disable")
            self.assertEqual(run("status").stdout.strip(), "disabled")
            self.assertEqual(subprocess.check_output(
                ["kreadconfig6", "--file", str(config), "--group", "Windows",
                 "--key", "ElectricBorderTiling"], text=True).strip(), "false")
            run("set-sensitivity", "0")
            self.assertEqual(run("sensitivity").stdout.strip(), "0")
            self.assertNotEqual(run("set-sensitivity", "101", check=False).returncode, 0)
            self.assertEqual(run("sensitivity").stdout.strip(), "0")
            run("enable")
            self.assertEqual(run("status").stdout.strip(), "enabled")


if __name__ == "__main__":
    unittest.main()
