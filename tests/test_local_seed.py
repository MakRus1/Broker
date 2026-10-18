"""Инициализация чистого стенда не создает драфты, совпадающие с дефолтами."""
import copy
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/local'))
from seed import initialize


def defaults():
    result = {}
    for path in sorted((ROOT / 'schemas/configs').glob('*/*.yaml')):
        schema = yaml.safe_load(path.read_text())
        value = schema['default']
        if schema['mode'] == 'conditional':
            value = {'clauses': [], 'default': value}
        result[schema['name']] = value
    return result


class LocalSeedTest(unittest.TestCase):
    def test_clean_database_keeps_defaults_without_publishing(self):
        initial = defaults()
        self.assertTrue(initial)
        admin = Mock()
        admin.current.side_effect = lambda name: {
            'value': copy.deepcopy(initial[name]), 'version': 0,
        }
        admin.publish.side_effect = AssertionError('Драфт без изменений запрещен')
        initialize(admin, initial)
        self.assertEqual(admin.current.call_count, len(initial))
        admin.publish.assert_not_called()

    def test_only_changed_values_are_published_and_repeat_is_noop(self):
        stored = {
            'UNCHANGED': {'value': {'enabled': False}, 'version': 0},
            'CHANGED': {'value': {'timeout': 100}, 'version': 4},
        }
        admin = Mock()
        admin.current.side_effect = lambda name: stored[name]

        def publish(name, value, version, description):
            self.assertEqual(version, stored[name]['version'])
            self.assertNotEqual(value, stored[name]['value'])
            stored[name] = {'value': value, 'version': version + 1}

        admin.publish.side_effect = publish
        values = {'UNCHANGED': {'enabled': False}, 'CHANGED': {'timeout': 200}}
        initialize(admin, values)
        initialize(admin, values)
        admin.publish.assert_called_once_with(
            'CHANGED', {'timeout': 200}, 4, 'Начальные значения локального стенда')

    def test_publication_failure_is_not_silenced(self):
        admin = Mock()
        admin.current.return_value = {'value': {'timeout': 100}, 'version': 0}
        admin.publish.side_effect = RuntimeError('Публикация не выполнена')
        with self.assertRaisesRegex(RuntimeError, 'Публикация не выполнена'):
            initialize(admin, {'CLIENT': {'timeout': 200}})
