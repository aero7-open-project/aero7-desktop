#!/usr/bin/env python3
"""Run with KWALLET_PRISTINE_DIR pointing at the verified upstream extraction."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class PrepareTests(unittest.TestCase):
    def setUp(self):
        self.integration = Path(__file__).resolve().parent
        self.pristine = Path(os.environ["KWALLET_PRISTINE_DIR"]).resolve()
        self.tmp = tempfile.TemporaryDirectory(prefix="aero7-wallet-prepare-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "source"
        self.daemon = self.root / "src/runtime/ksecretd"
        self.daemon.mkdir(parents=True)
        for name in ("CMakeLists.txt", "ksecretd.cpp", "knewwalletdialog.cpp"):
            shutil.copyfile(self.pristine / "src/runtime/ksecretd" / name, self.daemon / name)

    def snapshot(self):
        return {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                for p in self.daemon.iterdir() if p.is_file()}

    def run_prepare(self):
        return subprocess.run(["bash", str(self.integration / "prepare.sh"), str(self.root)],
                              capture_output=True, text=True, check=False)

    def test_applies_exact_patch_and_assets(self):
        result = self.run_prepare()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.daemon / "aero7vaultpresentation.h").read_bytes(),
                         (self.integration / "aero7vaultpresentation.h").read_bytes())
        self.assertEqual(self.snapshot()["credential-vault.png"],
                         "ee4f3b564acf5559cb73997e2ad9cd3a0c322b0f9b73ed3f0aaa4331c69b5d71")
        # Reversing precisely this patch must restore all three upstream files.
        result = subprocess.run(["patch", "--batch", "--reverse", "--fuzz=0", "-p1", "-d", str(self.root),
                                 "-i", str(self.integration / "aero7-vault-presentation.patch")],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for name in ("CMakeLists.txt", "ksecretd.cpp", "knewwalletdialog.cpp"):
            self.assertEqual((self.daemon / name).read_bytes(),
                             (self.pristine / "src/runtime/ksecretd" / name).read_bytes())

    def test_repeated_prepare_refuses_without_changes(self):
        self.assertEqual(self.run_prepare().returncode, 0)
        before = self.snapshot()
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.assertEqual(self.snapshot(), before)

    def test_modified_upstream_refuses_without_changes(self):
        with (self.daemon / "knewwalletdialog.cpp").open("a") as output:
            output.write("\n// pre-existing user change\n")
        before = self.snapshot()
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.assertEqual(self.snapshot(), before)

    def test_existing_overlay_refuses_without_changes(self):
        (self.daemon / "aero7vaultpresentation.h").write_text("existing overlay\n")
        before = self.snapshot()
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.assertEqual(self.snapshot(), before)

    def test_symlinked_source_refuses_without_changes(self):
        actual = self.daemon.with_name("original")
        self.daemon.rename(actual)
        self.daemon.symlink_to(actual, target_is_directory=True)
        before = self.snapshot()
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.assertEqual(self.snapshot(), before)


if __name__ == "__main__":
    unittest.main()
