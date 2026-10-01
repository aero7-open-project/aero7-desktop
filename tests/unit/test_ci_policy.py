"""Desktop GitHub checks must not duplicate the dedicated package builder."""
from pathlib import Path
import unittest

class SourceOnlyCI(unittest.TestCase):
    def test_no_compilation_or_packages(self):
        workflow = (Path(__file__).resolve().parents[2] / '.github/workflows/ci.yml').read_text()
        for command in ('cmake ', 'makepkg ', 'ninja ', 'pacman ', 'arch-package:'):
            self.assertNotIn(command, workflow)
        self.assertIn('contents: read', workflow)
        self.assertIn('bash tests/ci/validate-source.sh', workflow)
        self.assertIn('test_wallpaper_defaults.js', workflow)

if __name__ == '__main__':
    unittest.main()
