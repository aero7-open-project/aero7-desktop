#!/usr/bin/env python3

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MIGRATOR = ROOT / "migrations/aero7-migrate"


class MigrationTest(unittest.TestCase):
    def test_backup_is_idempotent_and_source_is_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config"
            state = root / "state"
            config.mkdir()
            source = config / "plasmashellrc"
            original = b"[PlasmaViews]\nversion=original\n"
            source.write_bytes(original)
            environment = os.environ.copy()
            environment["AERO7_CONFIG_HOME"] = str(config)
            environment["AERO7_STATE_HOME"] = str(state)

            first = subprocess.run(
                (str(MIGRATOR),), env=environment, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
            )
            first_payload = json.loads(first.stdout)
            marker = state / "aero7-desktop/migration.json"
            marker_before = marker.read_bytes()
            backup = Path(first_payload["backup"]) / "plasmashellrc"

            source.write_bytes(b"[PlasmaViews]\nversion=user-change\n")
            second = subprocess.run(
                (str(MIGRATOR),), env=environment, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
            )

            self.assertEqual(json.loads(second.stdout), first_payload)
            self.assertEqual(marker.read_bytes(), marker_before)
            self.assertEqual(backup.read_bytes(), original)
            self.assertEqual(source.read_bytes(), b"[PlasmaViews]\nversion=user-change\n")

    def test_aero_window_defaults_are_applied_without_losing_unrelated_settings(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config"
            state = root / "state"
            config.mkdir()
            kwinrc = config / "kwinrc"
            kwinrc.write_text("# keep this comment\n[Windows]\nFocusPolicy=ClickToFocus\n", encoding="utf-8")
            shortcuts = config / "kglobalshortcutsrc"
            shortcuts.write_text("[other]\nkey=value\n", encoding="utf-8")
            environment = os.environ.copy()
            environment["AERO7_CONFIG_HOME"] = str(config)
            environment["AERO7_STATE_HOME"] = str(state)

            result = subprocess.run(
                (str(MIGRATOR),), env=environment, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
            )
            payload = json.loads(result.stdout)
            migrated_kwin = kwinrc.read_text(encoding="utf-8")
            migrated_shortcuts = shortcuts.read_text(encoding="utf-8")
            self.assertIn("# keep this comment", migrated_kwin)
            self.assertIn("FocusPolicy=ClickToFocus", migrated_kwin)
            self.assertIn("library=org.smod.smod", migrated_kwin)
            self.assertIn("LayoutName=thumbnail_aero", migrated_kwin)
            self.assertIn("LayoutName=flip3d", migrated_kwin)
            self.assertIn("libkwin_effect_smodsnapEnabled=true", migrated_kwin)
            self.assertIn("aero7shakeEnabled=true", migrated_kwin)
            self.assertIn("aero7snapEnabled=true", migrated_kwin)
            self.assertIn("fadingpopupsaeroEnabled=false", migrated_kwin)
            self.assertIn("fadingpopupsEnabled=true", migrated_kwin)
            self.assertIn("squashaeroEnabled=false", migrated_kwin)
            self.assertIn("squashEnabled=true", migrated_kwin)
            self.assertIn("Walk Through Windows Alternative=Meta+Tab", migrated_shortcuts)
            self.assertTrue(payload["changes"])
            self.assertEqual(
                (Path(payload["backup"]) / "kwinrc").read_text(encoding="utf-8"),
                "# keep this comment\n[Windows]\nFocusPolicy=ClickToFocus\n",
            )


if __name__ == "__main__":
    unittest.main()
