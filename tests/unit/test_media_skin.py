#!/usr/bin/env python3
"""Keep the VLC skin self-contained and its XDG launch identity valid."""

from configparser import ConfigParser
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets/media-player"
DESKTOP = ROOT / "packaging/applications/aero7-media-player.desktop"


class MediaSkinTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.theme = ET.parse(ASSETS / "theme.xml").getroot()

    def test_every_referenced_bitmap_is_bundled(self):
        bitmaps = {node.get("id"): node for node in self.theme.findall("Bitmap")}
        bitmap_ids = set(bitmaps)
        bitmap_ids.update(node.get("id") for node in self.theme.iter("SubBitmap"))
        self.assertTrue(bitmaps)
        for name, node in bitmaps.items():
            file_name = node.get("file")
            self.assertFalse(Path(file_name).is_absolute(), name)
            self.assertNotIn("..", Path(file_name).parts, name)
            self.assertTrue((ASSETS / file_name).is_file(), name)
        for node in self.theme.iter():
            for attribute in ("image", "up", "up1", "up2", "down", "down1", "down2", "over", "over1", "over2"):
                reference = node.get(attribute)
                if reference and reference != "none":
                    self.assertIn(reference, bitmap_ids, (node.get("id"), attribute))

    def test_real_playback_and_queue_controls_exist(self):
        windows = {node.get("id") for node in self.theme.findall("Window")}
        self.assertEqual(windows, {"now", "queue"})
        self.assertIsNotNone(self.theme.find(".//Video"))
        self.assertIsNotNone(self.theme.find(".//Playtree"))
        actions = " ".join(node.get("action", "") for node in self.theme.iter())
        for action in ("vlc.stop()", "playlist.next()", "playlist.previous()", "queue.hide()"):
            self.assertIn(action, actions)
        checkbox_actions = " ".join(
            node.get(name, "") for node in self.theme.iter("Checkbox")
            for name in ("action1", "action2")
        )
        self.assertIn("vlc.play()", checkbox_actions)
        self.assertIn("vlc.pause()", checkbox_actions)
        self.assertIn("queue.show()", checkbox_actions)

    def test_desktop_file_opens_media_in_our_skin(self):
        parser = ConfigParser(interpolation=None, delimiters=("=",))
        parser.optionxform = str
        parser.read(DESKTOP)
        entry = parser["Desktop Entry"]
        self.assertEqual(entry["Name"], "Media Player")
        self.assertIn("-I skins2", entry["Exec"])
        self.assertIn("aero7-media-player.vlt", entry["Exec"])
        self.assertIn("%U", entry["Exec"])
        self.assertEqual(entry["Icon"], "aero7-media-player")
        mimes = set(filter(None, entry["MimeType"].split(";")))
        for mime in ("audio/mpeg", "audio/x-m4a", "audio/mp4", "video/mp4"):
            self.assertIn(mime, mimes)
        self.assertFalse(re.search(r"(?:/usr/share/icons|Breeze|Adwaita)", entry["Exec"]))


if __name__ == "__main__":
    unittest.main()
