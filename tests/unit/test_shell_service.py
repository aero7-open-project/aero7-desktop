#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "services/session/aero7-shell-service"


def load_source(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        from importlib.machinery import SourceFileLoader
        loader = SourceFileLoader(name, str(path))
        spec = importlib.util.spec_from_loader(name, loader)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


service = load_source("aero7_shell_service", SOURCE)


class ShellServiceTest(unittest.TestCase):
    def test_atomic_json_replaces_complete_document(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "health.json"
            service.atomic_json(target, {"schema": 1, "healthy": True})
            self.assertEqual(
                json.loads(target.read_text(encoding="utf-8")),
                {"schema": 1, "healthy": True},
            )
            self.assertFalse(target.with_suffix(".json.new").exists())

    def test_snapshot_reports_real_backends_without_fake_state(self):
        with mock.patch.object(service, "shell_name", return_value=service.EXPECTED_SHELL), \
             mock.patch.object(service, "layout_state", return_value={"ready": True, "valid": True}):
            current = service.snapshot()
        self.assertEqual(current["schema"], 2)
        self.assertIn("kwin_wayland", current["binaries"])
        self.assertIn("plasmashell", current["processes"])
        self.assertIsInstance(current["processes"]["kwin_wayland"], bool)
        self.assertFalse(current["stock_fallback_allowed"])

    def test_failure_report_explicitly_forbids_stock_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(service.shutil, "which", return_value=None):
                service.report_shell_failure(root, "test failure")
            recovery = json.loads((root / "recovery.json").read_text(encoding="utf-8"))
            self.assertIn("stock Plasma panels are forbidden", recovery["fallback"])

    def test_layout_requires_one_aero_panel_per_screen(self):
        with mock.patch.object(service.subprocess, "run") as run:
            run.return_value.returncode = 0
            run.return_value.stdout = json.dumps({
                "panels": 2,
                "screens": 2,
                "panelTypes": [service.EXPECTED_PANEL, service.EXPECTED_PANEL],
                "desktops": 2,
                "desktopTypes": [service.EXPECTED_DESKTOP, service.EXPECTED_DESKTOP],
            })
            self.assertTrue(service.layout_state()["valid"])

    def test_appearance_state_detects_a_global_theme_override(self):
        def config_read(command, **_kwargs):
            key = (command[2], command[4], command[6])
            if key == ("kdeglobals", "General", "ColorScheme"):
                return mock.Mock(returncode=0, stdout="Aero\n")
            value = service.expected_appearance("Aero")[key]
            if key == ("kdeglobals", "Icons", "Theme"):
                value = "breeze-dark"
            return mock.Mock(returncode=0, stdout=value + "\n")

        with mock.patch.object(service.subprocess, "run", side_effect=config_read):
            state = service.appearance_state()
        self.assertFalse(state["valid"])
        self.assertEqual(state["values"]["kdeglobals:Icons:Theme"], "breeze-dark")

    def test_appearance_state_tracks_the_aero_kvantum_theme(self):
        self.assertEqual(
            service.expected_appearance("Aero")[
                ("Kvantum/kvantum.kvconfig", "General", "theme")
            ],
            "Windows7Aero",
        )
        self.assertEqual(
            service.expected_appearance("BreezeDark")[
                ("Kvantum/kvantum.kvconfig", "General", "theme")
            ],
            "KvDark",
        )


if __name__ == "__main__":
    unittest.main()
