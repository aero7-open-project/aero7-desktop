#!/usr/bin/env python3
import configparser
import importlib.machinery
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[2] / "services/session/aero7-portal-policy"
loader = importlib.machinery.SourceFileLoader("portal_policy", str(SOURCE))
spec = importlib.util.spec_from_loader(loader.name, loader)
policy = importlib.util.module_from_spec(spec)
loader.exec_module(policy)


class PortalPolicyTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.runtime = self.root / "runtime"
        self.runtime.mkdir(mode=0o700)
        self.env = {
            "HOME": str(self.root / "home"),
            "XDG_CONFIG_HOME": str(self.root / "user"),
            "XDG_DATA_HOME": str(self.root / "data"),
            "XDG_CONFIG_DIRS": str(self.root / "system"),
            "XDG_DATA_DIRS": str(self.root / "vendor"),
            "XDG_RUNTIME_DIR": str(self.runtime),
            "XDG_CURRENT_DESKTOP": "Aero7:KDE",
        }
        self.original = ("[preferred]\ndefault=kde\n"
                         "org.freedesktop.impl.portal.Secret=kwallet\n"
                         "org.freedesktop.impl.portal.Settings=kde;gtk;\n"
                         "org.freedesktop.impl.portal.Notification=plasmanotify\n")
        self.source = self.write("system", "kde-portals.conf", self.original)

    def write(self, root, filename, value):
        path = self.root / root / "xdg-desktop-portal" / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)
        return path

    def test_disabled_changes_only_secret_and_preserves_source(self):
        prefix = policy.prepare(self.env, "false\n")
        self.assertIsNotNone(prefix)
        path = prefix / "xdg-desktop-portal/aero7-portals.conf"
        actual = configparser.ConfigParser()
        actual.read(path)
        expected = configparser.ConfigParser()
        expected.read_string(self.original)
        expected["preferred"]["org.freedesktop.impl.portal.Secret"] = "none"
        self.assertEqual(dict(actual["preferred"]), dict(expected["preferred"]))
        self.assertEqual(self.source.read_text(), self.original)
        self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_enabled_or_unknown_is_unchanged(self):
        for value in ("true", "true\n", "", "invalid"):
            self.assertIsNone(policy.prepare(self.env, value))
        self.assertFalse((self.runtime / "aero7-desktop").exists())

    def test_policy_survives_startplasma_desktop_identity(self):
        prefix = policy.prepare(self.env, "false")
        # startplasma-wayland replaces Aero7:KDE with KDE before starting portals.
        started = dict(self.env, XDG_CURRENT_DESKTOP="KDE",
                       XDG_CONFIG_DIRS=str(prefix) + ":" + self.env["XDG_CONFIG_DIRS"])
        selected, user_owned = policy.selected_config(started)
        self.assertEqual(selected, prefix / "xdg-desktop-portal/kde-portals.conf")
        self.assertFalse(user_owned)
        self.assertEqual(selected.read_bytes(),
                         (prefix / "xdg-desktop-portal/aero7-portals.conf").read_bytes())
        self.assertEqual(selected.stat().st_mode & 0o777, 0o600)

    def test_user_portal_override_wins(self):
        for filename in ("aero7-portals.conf", "portals.conf"):
            path = self.write("user", filename, self.original)
            self.assertIsNone(policy.prepare(self.env, "false"))
            self.assertEqual(path.read_text(), self.original)
            path.unlink()

    def test_user_data_override_wins_over_vendor(self):
        self.source.unlink()
        path = self.write("data", "aero7-portals.conf", self.original)
        self.assertEqual(policy.selected_config(self.env), (path, True))
        self.assertIsNone(policy.prepare(self.env, "false"))

    def test_other_secret_backends_and_fallbacks_preserved(self):
        for value in ("gnome-keyring", "none", "kwallet;gnome-keyring;"):
            self.source.write_text(self.original.replace("Secret=kwallet", "Secret=" + value))
            self.assertIsNone(policy.prepare(self.env, "false"))

    def test_optional_package_config_and_enabled_policy(self):
        optional = self.write("system", "aero7-portals.conf", self.original)
        self.assertEqual(policy.selected_config(self.env), (optional, False))
        self.assertIsNone(policy.prepare(self.env, "true"))
        # An explicit user Wallet/Enabled=false still takes precedence.
        self.assertIsNotNone(policy.prepare(self.env, "false"))

    def test_unsafe_or_missing_runtime_not_used(self):
        os.chmod(self.runtime, 0o755)
        self.assertIsNone(policy.prepare(self.env, "false"))
        del self.env["XDG_RUNTIME_DIR"]
        self.assertIsNone(policy.prepare(self.env, "false"))

    def test_invalid_config_not_replaced(self):
        self.source.write_text("not an ini file")
        with self.assertRaises(configparser.Error):
            policy.prepare(self.env, "false")
        self.assertEqual(self.source.read_text(), "not an ini file")


if __name__ == "__main__":
    unittest.main()
