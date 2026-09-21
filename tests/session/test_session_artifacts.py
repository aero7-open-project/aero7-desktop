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
        files.extend((ROOT / "tests" / "visual").glob("*.desktop"))
        files.extend((ROOT / "packaging" / "applications").glob("*.desktop"))
        for path in files:
            subprocess.run(
                (validator, str(path)),
                check=True,
            )

    def test_baloo_worker_has_a_hidden_portal_identity(self):
        desktop = self.read_desktop_path(
            ROOT / "packaging/applications/org.kde.baloo.desktop"
        )
        self.assertEqual(desktop["Type"], "Application")
        self.assertEqual(desktop["Exec"], "/usr/lib/kf6/baloo_file_extractor")
        self.assertEqual(desktop["TryExec"], desktop["Exec"])
        self.assertEqual(desktop["NoDisplay"], "true")
        self.assertEqual(desktop["Terminal"], "false")
        self.assertEqual(desktop["StartupNotify"], "false")
        self.assertNotIn("Hidden", desktop)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("install(FILES packaging/applications/org.kde.baloo.desktop", cmake)
        self.assertFalse((ROOT / "packaging/autostart/org.kde.baloo.desktop").exists())

    def test_file_explorer_route_uses_the_full_fork_identity(self):
        package = (ROOT / "packaging/arch/PKGBUILD").read_text(encoding="utf-8")
        self.assertIn("'aero7-file-explorer'", package)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("org.aero7.fileexplorer.desktop", cmake)
        layout = (ROOT / "services/session/aero7-shell-layout.js").read_text(
            encoding="utf-8"
        )
        self.assertIn("applications:org.aero7.FileExplorer.desktop", layout)

    def test_retired_native_surfaces_are_not_reintroduced(self):
        for surface in ("start", "taskbar", "tray", "desktop", "controlpanel", "notifications"):
            self.assertFalse(any((ROOT / "shell" / surface).glob("*")), surface)

    def test_package_uses_current_optional_dependency_names(self):
        package = (ROOT / "packaging/arch/PKGBUILD").read_text(encoding="utf-8")
        self.assertIn("'aero7-device-manager'", package)
        required, optional = package.split("optdepends=(", 1)
        self.assertNotIn("'aero7-programs-center-git'", required)
        self.assertIn("aero7-programs-center-git: optional", optional)

    def test_new_sessions_use_the_windows_7_factory_pin_order(self):
        layout = (ROOT / "services/session/aero7-shell-layout.js").read_text(
            encoding="utf-8"
        )
        factory_block = layout.split("var defaultLaunchers = [", 1)[1].split("]", 1)[0]
        self.assertLess(
            factory_block.index('"applications:qterminal.desktop"'),
            factory_block.index('"applications:org.aero7.FileExplorer.desktop"'),
        )
        self.assertTrue(factory_block.rstrip().endswith("internetExplorerLauncher"))
        self.assertIn("canonicalLaunchers = savedLaunchers", layout)
        self.assertIn("windows7FactoryPinLayoutMigrated", layout)
        self.assertIn("isPreviousFactoryLayout(canonicalLaunchers)", layout)
        package = (ROOT / "packaging/arch/PKGBUILD").read_text(encoding="utf-8")
        self.assertIn("'aero7-internet-explorer'", package)
        companion = ROOT / "companions/aero7-internet-explorer"
        self.assertTrue((companion / "src/InternetExplorer.cpp").is_file())
        desktop = self.read_desktop_path(
            companion / "data/aero7-internet-explorer.desktop"
        )
        self.assertEqual(desktop["Name"], "Internet Explorer")
        self.assertEqual(desktop["Exec"], "aero7-internet-explorer %U")
        self.assertEqual(desktop["StartupNotify"], "false")

    def test_brightness_is_not_a_permanent_taskbar_tray_item(self):
        layout = (ROOT / "services/session/aero7-shell-layout.js").read_text(
            encoding="utf-8"
        )
        self.assertIn('"org.kde.plasma.brightness"', layout)
        self.assertIn("withoutDisabledTrayItems", layout)
        self.assertIn('tray.writeConfig("extraItems"', layout)

    def test_theme_changes_preserve_aero_appearance_components(self):
        setup = (ROOT / "services/session/aero7-session-setup").read_text(
            encoding="utf-8"
        )
        unit = (ROOT / "packaging/systemd/aero7-session-setup.service").read_text(
            encoding="utf-8"
        )
        self.assertIn("Wants=plasma-plasmashell.service", unit)
        self.assertNotIn("Requires=plasma-plasmashell.service", unit)
        self.assertIn("LookAndFeelPackage authui7", setup)
        self.assertIn("widgetStyle kvantum", setup)
        self.assertIn("Theme 'Windows 7 Aero'", setup)
        self.assertIn('kvantummanager --set "$kvantum_theme"', setup)
        self.assertIn("kvantum_theme=KvDark", setup)
        self.assertIn("kvantum_theme=Windows7Aero", setup)
        self.assertNotIn(
            '[[ ! -e "$state_home/visual-defaults-v2.applied" ]]', setup
        )
        self.assertIn("reload_aeroshell_after_appearance_repair", setup)
        self.assertIn("if ((reconcile_only && !layout_only)); then", setup)
        self.assertIn(
            "systemctl --user restart plasma-plasmashell.service", setup
        )
        self.assertIn(
            "systemctl --user reset-failed plasma-plasmashell.service", setup
        )
        repair = setup.split(
            "reload_aeroshell_after_appearance_repair()", 1
        )[1]
        self.assertLess(
            repair.index("systemctl --user restart plasma-plasmashell.service"),
            repair.index("wait_for_aeroshell"),
        )
        self.assertLess(
            repair.index("wait_for_aeroshell"),
            repair.index("org.kde.PlasmaShell.evaluateScript"),
        )

    def test_fresh_desktop_contains_only_the_recycle_bin_template(self):
        template = self.read_desktop_path(ROOT / "defaults/Recycle Bin.desktop")
        self.assertEqual(template["Type"], "Link")
        self.assertEqual(template["URL"], "trash:/")
        self.assertEqual(template["Icon"], "user-trash")
        setup = (ROOT / "services/session/aero7-session-setup").read_text(
            encoding="utf-8"
        )
        self.assertIn("fresh-desktop-v1.applied", setup)
        self.assertIn("never delete or replace files", setup)
        gadget_manager = (
            ROOT / "companions/aero7-gadgets/src/GadgetManager.cpp"
        ).read_text(encoding="utf-8")
        restore = gadget_manager.split("void GadgetManager::restoreSession()", 1)[1].split(
            "void GadgetManager::saveSession()", 1
        )[0]
        self.assertNotIn("org.aero7.gadgets.clock", restore)
        self.assertNotIn("org.aero7.gadgets.weather", restore)

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
