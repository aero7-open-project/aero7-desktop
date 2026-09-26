#!/usr/bin/env python3
"""Media defaults must not overwrite an explicit user application choice."""

import importlib.machinery
import importlib.util
from pathlib import Path
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2] / "services/session/aero7-media-associations"
loader = importlib.machinery.SourceFileLoader("aero7_media_associations", str(SOURCE))
spec = importlib.util.spec_from_loader(loader.name, loader)
module = importlib.util.module_from_spec(spec)
loader.exec_module(module)


class MediaAssociationsTest(unittest.TestCase):
    def test_only_exact_legacy_launcher_is_hidden_and_backed_up(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "vlc.desktop"
            legacy = ("[Desktop Entry]\nType=Application\nName=Media Player\n"
                      "GenericName=Media Player\nComment=Play audio and video\n"
                      "Exec=/usr/bin/vlc --started-from-file %U\nTryExec=/usr/bin/vlc\n"
                      "Icon=audio-player\nCategories=AudioVideo;Player;\n"
                      "Keywords=media;player;audio;video;music;movie;vlc;\n"
                      "StartupNotify=true\nTerminal=false\n")
            path.write_text(legacy)
            self.assertTrue(module.hide_legacy_vlc_launcher(path))
            self.assertEqual(path.read_text(), legacy + "NoDisplay=true\n")
            self.assertEqual(path.with_name("vlc.desktop.aero7-pre-skin").read_text(), legacy)
            self.assertFalse(module.hide_legacy_vlc_launcher(path))

    def test_custom_legacy_launcher_is_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "vlc.desktop"
            path.write_text("[Desktop Entry]\nName=My VLC\nExec=vlc %U\n")
            self.assertFalse(module.hide_legacy_vlc_launcher(path))
            self.assertFalse(path.with_name("vlc.desktop.aero7-pre-skin").exists())

    def test_existing_choices_are_preserved_and_missing_types_added(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "mimeapps.list"
            original = (
                "[Default Applications]\n"
                "audio/mpeg=audacious.desktop\n"
                "x-scheme-handler/https=firefox.desktop\n"
                "\n[Added Associations]\n"
                "audio/mpeg=audacious.desktop;vlc.desktop;\n"
            )
            path.write_text(original)
            self.assertTrue(module.ensure_defaults(path))
            updated = path.read_text()
            self.assertIn("audio/mpeg=audacious.desktop\n", updated)
            self.assertIn("x-scheme-handler/https=firefox.desktop\n", updated)
            self.assertIn("audio/mp4=aero7-media-player.desktop\n", updated)
            self.assertIn("[Added Associations]\naudio/mpeg=audacious.desktop;vlc.desktop;", updated)
            self.assertFalse(module.ensure_defaults(path))
            self.assertEqual(updated, path.read_text())

    def test_new_file_gets_all_player_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "mimeapps.list"
            self.assertTrue(module.ensure_defaults(path))
            lines = path.read_text().splitlines()
            self.assertEqual(lines[0], "[Default Applications]")
            for mime in module.MIME_TYPES:
                self.assertIn(f"{mime}={module.DESKTOP_ID}", lines)


if __name__ == "__main__":
    unittest.main()
