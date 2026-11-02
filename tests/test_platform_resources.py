"""Профиль владельца: слияние окружений, строгие имена и ограничения ресурсов."""

from pathlib import Path
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from platform_resources import load


class PlatformResourcesTest(unittest.TestCase):
    def profile(self):
        return yaml.safe_load((ROOT / 'services/finance/platform.yaml').read_text())

    def parse(self, profile, environment='staging'):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'platform.yaml'
            path.write_text(yaml.safe_dump(profile))
            return load(path, environment, postgres=True)

    def test_environment_overrides_only_specified_fields(self):
        profile = self.profile()
        profile['environments']['production'] = {
            'application': {'replicas': 2},
            'postgres': {
                'instances': 3,
                'resources': {'limits': {'memory': '2Gi'}},
                'storage': '20Gi',
            },
        }
        result = self.parse(profile, 'production')
        self.assertEqual(result['postgres']['instances'], 3)
        self.assertEqual(result['postgres']['resources']['requests']['cpu'], '100m')
        self.assertEqual(result['postgres']['resources']['limits']['memory'], '2Gi')
        self.assertEqual(result['application']['replicas'], 2)
        self.assertEqual(self.parse(profile)['postgres']['instances'], 1)

    def test_invalid_profiles_do_not_silently_deploy_defaults(self):
        for override in (
            {'postgres': {'instances': True}},
            {'postgres': {'instances': 0}},
            {'postgres': {'replicas': 3}},
            {'postgres': {'storage': '0Gi'}},
            {'postgres': {'resources': {'limits': {'memory': '64Mi'}}}},
            {'application': {'resources': {'limits': {'cpu': '0'}}}},
            {'typo': {}},
        ):
            with self.subTest(override=override):
                profile = self.profile()
                profile['environments']['staging'] = override
                with self.assertRaises(ValueError):
                    self.parse(profile)
        with self.assertRaisesRegex(ValueError, 'нет профиля'):
            self.parse(self.profile(), 'missing')
