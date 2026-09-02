#!/usr/bin/env python3

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
FORBIDDEN = (
    "aero_desktop/",
    "aero7-shell/install.sh",
    "source ../aero_desktop",
    ". ../aero_desktop",
)


class SourcePolicyTest(unittest.TestCase):
    def test_legacy_script_tree_is_not_imported_or_invoked(self):
        violations = []
        for path in ROOT.rglob("*"):
            relative = path.relative_to(ROOT)
            generated_package_tree = (
                len(relative.parts) >= 3
                and relative.parts[:2] == ("packaging", "arch")
                and relative.parts[2] in {"src", "pkg"}
            )
            if (not path.is_file() or ".git" in path.parts
                    or "build" in path.parts or generated_package_tree):
                continue
            if path.resolve() == Path(__file__).resolve():
                continue
            try:
                content = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            for forbidden in FORBIDDEN:
                if forbidden in content:
                    violations.append(f"{path.relative_to(ROOT)}: {forbidden}")
        self.assertEqual(violations, [])


if __name__ == "__main__":
    unittest.main()
