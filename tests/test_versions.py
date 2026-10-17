"""Защита от рассогласования закрепленных версий и шаблонов."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent


class VersionsTest(unittest.TestCase):
    def test_python_tools_match_version_manifest(self):
        versions = dict(line.split('=', 1) for line in (ROOT / 'tools/versions.env').read_text().splitlines() if line and not line.startswith('#'))
        requirements = (ROOT / 'requirements.txt').read_text()
        self.assertIn('cmake==' + versions['CMAKE_VERSION'], requirements)
        self.assertIn('ninja==' + versions['NINJA_VERSION'], requirements)
        self.assertIn('clang-format==' + versions['CLANG_FORMAT_VERSION'], requirements)
        self.assertIn('google-re2==' + versions['GOOGLE_RE2_VERSION'], requirements)
        self.assertEqual(len(versions['USERVER_COMMIT']), 40)
        self.assertEqual(len(versions['USERVER_ARCHIVE_SHA256']), 64)

    def test_template_uses_current_platform_functions(self):
        text = (ROOT / 'scripts/templates/service/CMakeLists.txt').read_text()
        self.assertIn('broker_openapi_generate', text)
        self.assertIn('broker_common', text)
        self.assertNotIn('include_directories(src)', text)
