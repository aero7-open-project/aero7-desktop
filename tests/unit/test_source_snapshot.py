#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("snapshot", ROOT / "packaging/create-source-snapshot.py")
snapshot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(snapshot)


class SourceSnapshotTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "source"
        self.root.mkdir()
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        (self.root / "CMakeLists.txt").write_text("project(fixture)\n")
        (self.root / "LICENSE").write_text("fixture license\n")
        (self.root / ".gitignore").write_text("/build/\n/packaging/arch/src/\n*.pkg.tar.zst\n")
        subprocess.run(["git", "-C", str(self.root), "add", "."], check=True)

    def test_preserves_edits_new_files_modes_and_deletions(self):
        (self.root / "old.txt").write_text("old\n")
        subprocess.run(["git", "-C", str(self.root), "add", "old.txt"], check=True)
        (self.root / "old.txt").unlink()
        (self.root / "LICENSE").write_text("edited license\n")
        (self.root / "new-script").write_text("#!/bin/sh\nexit 0\n")
        (self.root / "new-script").chmod(0o755)
        for name in ("build/compiled", "packaging/arch/src/nested.tar.gz", "old.pkg.tar.zst"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("ignored generated file\n")
        output = self.base / "source.tar.gz"
        snapshot.create_snapshot(self.root, output, "aero7-desktop-0.2.0")
        with tarfile.open(output) as archive:
            names = archive.getnames()
            self.assertEqual(len(names), 4)
            self.assertNotIn("aero7-desktop-0.2.0/old.txt", names)
            self.assertEqual(archive.extractfile("aero7-desktop-0.2.0/LICENSE").read(), b"edited license\n")
            script = archive.getmember("aero7-desktop-0.2.0/new-script")
            self.assertEqual(script.mode, 0o755)
            self.assertEqual((script.uid, script.gid, script.mtime), (0, 0, 0))

    def test_identical_inputs_produce_identical_archives(self):
        first, second = self.base / "one.tar.gz", self.base / "two.tar.gz"
        snapshot.create_snapshot(self.root, first, "source")
        snapshot.create_snapshot(self.root, second, "source")
        self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_rejects_tracked_generated_content(self):
        path = self.root / "build" / "compiled"
        path.parent.mkdir()
        path.write_text("generated\n")
        subprocess.run(["git", "-C", str(self.root), "add", "-f", "build/compiled"], check=True)
        with self.assertRaisesRegex(ValueError, "Generated or unsafe"):
            snapshot.create_snapshot(self.root, self.base / "bad.tar.gz", "source")

    def test_rejects_escaping_symlink(self):
        (self.base / "external").write_text("not source\n")
        (self.root / "escape").symlink_to("../external")
        with self.assertRaisesRegex(ValueError, "escapes"):
            snapshot.create_snapshot(self.root, self.base / "bad.tar.gz", "source")

    def test_preserves_internal_relative_symlink(self):
        (self.root / "license-link").symlink_to("LICENSE")
        output = self.base / "links.tar.gz"
        snapshot.create_snapshot(self.root, output, "source")
        with tarfile.open(output) as archive:
            entry = archive.getmember("source/license-link")
            self.assertTrue(entry.issym())
            self.assertEqual(entry.linkname, "LICENSE")

    def test_refuses_overwrite_and_output_inside_source(self):
        output = self.base / "existing.tar.gz"
        output.write_bytes(b"preserve me")
        with self.assertRaises(FileExistsError):
            snapshot.create_snapshot(self.root, output, "source")
        self.assertEqual(output.read_bytes(), b"preserve me")
        with self.assertRaisesRegex(ValueError, "outside"):
            snapshot.create_snapshot(self.root, self.root / "new.tar.gz", "source")

    def test_requires_exact_repo_root_and_safe_prefix(self):
        nested = self.root / "nested"
        nested.mkdir()
        with self.assertRaisesRegex(ValueError, "root of its own"):
            snapshot.create_snapshot(nested, self.base / "bad.tar.gz", "source")
        for prefix in ("", ".", "..", "../escape", "/absolute", "one/two", "one\\two"):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                snapshot.create_snapshot(self.root, self.base / "bad.tar.gz", prefix)


if __name__ == "__main__":
    unittest.main()
