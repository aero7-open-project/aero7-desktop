#!/usr/bin/env python3
"""Exercise the real fontconfig configuration order without changing the host."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
RULE = ROOT / "defaults/60-aero7-monospace.conf"


class MonospaceDefaultTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="aero7-fontconfig-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.user = self.work / "user"
        self.user.mkdir()
        configs = self.work / "conf.d"
        configs.mkdir()
        for source in Path("/etc/fonts/conf.d").glob("*.conf"):
            if source.name != RULE.name:
                (configs / source.name).symlink_to(source.resolve())
        shutil.copyfile(RULE, configs / RULE.name)
        tree = ET.parse("/etc/fonts/fonts.conf")
        includes = [node for node in tree.findall("include")
                    if (node.text or "").strip() == "conf.d"]
        self.assertEqual(len(includes), 1, "Unexpected fontconfig include layout")
        includes[0].text = str(configs)
        config = self.work / "fonts.conf"
        tree.write(config, encoding="utf-8", xml_declaration=True)
        self.env = {**os.environ, "FONTCONFIG_FILE": str(config),
                    "XDG_CONFIG_HOME": str(self.user)}

    def match(self, family, env=None):
        return subprocess.check_output(
            ["fc-match", "-f", "%{family[0]}", family],
            env=env or self.env, text=True)

    def test_generic_is_fixed_family(self):
        for family in ("monospace", "Monospace", "monospace:weight=bold"):
            self.assertEqual(self.match(family), "DejaVu Sans Mono")

    def test_explicit_family_is_preserved(self):
        for family in ("Liberation Mono", "Noto Sans Mono", "DejaVu Sans Mono"):
            self.assertEqual(self.match(family), family)

    def test_unrelated_ui_families_are_unchanged(self):
        baseline = {**self.env}
        baseline.pop("FONTCONFIG_FILE")
        for family in ("sans-serif", "serif", "Noto Sans"):
            self.assertEqual(self.match(family), self.match(family, baseline))

    def test_user_monospace_alias_wins(self):
        user_fonts = self.user / "fontconfig"
        user_fonts.mkdir()
        (user_fonts / "fonts.conf").write_text(
            '<fontconfig><alias binding="same"><family>monospace</family>'
            '<prefer><family>Liberation Mono</family></prefer></alias></fontconfig>')
        self.assertEqual(self.match("Monospace"), "Liberation Mono")


if __name__ == "__main__":
    unittest.main()
